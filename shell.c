#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include<sys/wait.h>
#include <errno.h>
#include <asm/errno.h>
#include <limits.h>
#include <pwd.h>

typedef struct {
  char *history[200], *jobs[200], currentPath[PATH_MAX];
  int history_pid[200], jobs_pid[200], history_index, jobs_index, history_pid_index, jobs_pid_index;
} Manager;

void runShell();
void readLine(char[200]);
int parseLine(char[200], char[200], char *[200], int *);
void commandExcecute(char[200], char *[200], int, Manager *);
void freeArgs(char *[200], int);
int saveCommand(char[200], Manager *, int);
void initialManager(Manager *);
void excecuteHistory(Manager *);
void excecuteJobs(Manager *);
int excecuteCd(char *[200], int, Manager *);
int isProcessAlive(pid_t);
void freeManager(Manager *);
void restoreLine(char[200], char[200], int);
void restoreSpaces(char *);
int allocateMemory(char *[200], char *, int);
int removeIfEcho(char[200]);
void removeChar(char *, char);

int main() {
  setbuf(stdout,NULL);
  runShell();
  return 0;
}

/*
* inside the runshell there is a loop that provide the shell functionality by using
 * several function.
 * the function print promt and then the user is asked to write a command.
 * then the function parse the line the user wrote and separate the command from the arguments,
 * and execute the command.
 * the loop ends when the user write the exit command or when there is a problem with the chdir function.
*/
void runShell() {
  char line[200], command[200], *args[200];
  Manager manager;
  initialManager(&manager);
  int foreground = 0, numOfArgs;
  while (1) {
    readLine(line);
    foreground = parseLine(line, command, args, &numOfArgs);
    //if there was problem with memory allocation
    if (foreground == -1) {
      freeManager(&manager);
      exit(1);
    }
    //save the the command the user wrote for history and jobs
    if (saveCommand(line, &manager, foreground)) {
      //if there was problem with memory allocation
      freeArgs(args, numOfArgs);
      exit(1);
    }
    if (!strcmp(command, "exit")) {
      freeManager(&manager);
      freeArgs(args, numOfArgs);
      printf("%d\n", getpid());
      break;
    }
    //execute the appropriate command
    if (!strcmp(command, "history")) {
      excecuteHistory(&manager);
    } else if (!strcmp(command, "jobs")) {
      excecuteJobs(&manager);
    } else if (!strcmp(command, "cd")) {
      if (excecuteCd(args, numOfArgs, &manager) == -1)
        break;
    } else {
      commandExcecute(command, args, foreground, &manager);
    }
    freeArgs(args, numOfArgs);
  }
}

//prints the promt and read the line
void readLine(char line[200]) {
  do {
    printf("> ");
    fgets(line, 200, stdin);
  } while (strcmp(line, "\n") == 0);
}

//separate the line into command and arguments and allocate memory.
int parseLine(char line[200], char command[200], char *args[200], int *numOffArgs) {
  int i = 0, ret = 1, isEcho;
  char *token, tempLine[200], delimiter[6] = " \n";
  //remove the " " if the command is echo
  isEcho = removeIfEcho(line);
  strcpy(tempLine, line);
  //remove Apostrophes if the command is echo
  if (isEcho) {
    removeChar(line, '"');
    removeChar(line, '\'');
    removeChar(line, '\\');
  }
  token = strtok(line, delimiter);
  //initial the arguments
  while (token != NULL) {
    //if memory allocation went wrong
    if (allocateMemory(args, token, i) == -1)
      return -1;
    if (isEcho)
      restoreSpaces(args[i]);
    token = strtok(NULL, delimiter);
    i++;
  }
  //0 is the sign to background command
  if (i > 0 && !strcmp(args[i - 1], "&")) {
    ret = 0;
    free(args[i]);
    i--;
  }
  args[i] = NULL;
  //the command is the first argument
  if (i != 0)
    strcpy(command, args[0]);
  *numOffArgs = i;
  restoreLine(line, tempLine, isEcho);
  return ret;
}

//check if the command is echo and it is replaces all the spaces between " " so it wont disappear in the token command
int removeIfEcho(char line[200]) {
  char tempLine[200];
  strcpy(tempLine, line);
  int i = 0, isEcho = 0, flag1 = 0, flag2 = 0;
  char *token = strtok(tempLine, " \n");
  while (token != NULL) {
    if (i == 0 && !strcmp(token, "echo")) {
      isEcho = 1;
    }
    token = strtok(NULL, " \n");
    i++;
  }
  //replaces the spaces between the " " and the ' '
  if (isEcho) {
    for (i = 0; line[i] != 0; i++) {
      if (line[i] == '"' && flag1 == 0) {
        flag1 = 1;
        continue;
      }
      if (line[i] == '"' && flag1 == 1) {
        flag1 = 0;
        continue;
      }
      if (line[i] == '\'' && flag2 == 0) {
        flag2 = 1;
        continue;
      }
      if (line[i] == '\'' && flag2 == 1) {
        flag2 = 0;
        continue;
      }
      if (flag1 == 1 || flag2 == 1) {
        if (line[i] == ' ')
          line[i] = 1;
      }
    }
  }
  return isEcho;
}

//restore the spaces between the " "
void restoreSpaces(char *arg) {
  int i;
  for (i = 0; arg[i] != 0; i++) {
    if (arg[i] == 1) {
      arg[i] = ' ';
    }
  }
}

//remove the char c from line
void removeChar(char *line, char c) {
  int flag = 0;
  char *pr = line, *pw = line;
  while (*pr) {
    *pw = *pr++;
    char check = *pw;
    if (((*pw != c) || flag))
      pw++;
    if (check == '\\' && flag == 1)
      flag = 0;
    else if (check == '\\')
      flag = 1;
    else
      flag = 0;
  }
  *pw = '\0';
}

//assign the line the user wrote without the "&" if exist
void restoreLine(char line[200], char tempLine[200], int isEcho) {
  char *token;
  int length;
  strcpy(line, "");
  token = strtok(tempLine, " \n");
  while (token != NULL && strcmp("&", token)) {
    if (isEcho)
      restoreSpaces(token);
    strcat(line, token);
    strcat(line, " ");
    token = strtok(NULL, " \n");
  }
  length = strlen(line);
  if (length > 0)
    length -= 1;
  line[length] = 0;
}

//save the command the user wrote so we can use them late in history and jobs commands
int saveCommand(char line[200], Manager *manager, int foreground) {
  manager->history[(manager->history_index)] = strdup(line);
  //if the memory allocation went wrong
  if (manager->history[(manager->history_index)] == NULL) {
    freeManager(manager);
    return -1;
  }
  (manager->history_index)++;
  if (!foreground) {
    manager->jobs[manager->jobs_index] = strdup(line);
    //if the memory allocation went wrong
    if (manager->jobs[manager->jobs_index] == NULL) {
      freeManager(manager);
      return -1;
    }
    manager->jobs_index++;
  }
  return 0;
}

//execute the command the user wrote and save the process id
void commandExcecute(char command[200], char *args[200], int foreground, Manager *manager) {
  int stat;
  pid_t pid;
  if ((pid = fork()) == -1) {
    fprintf(stderr, "Error in system call\n");
  } else if (pid != 0) {
    //prevents zombies
    signal(SIGCHLD, SIG_IGN);
    //the father responsible to save the pid of the child
    manager->history_pid[manager->history_pid_index] = (int) pid;
    manager->history_pid_index++;
    //in foreground command the father needs to wait to the child
    if (foreground) {
      waitpid(pid, &stat, 0);
    } else {
      manager->jobs_pid[manager->jobs_pid_index] = (int) pid;
      manager->jobs_pid_index++;
    }
  } else {
    printf("%d\n", getpid());
    sleep(0.5);
    int check = execvp(command, args);
    if (check) {
      fprintf(stderr, "Error in system call\n");
      exit(0);
    }
  }
}

//free the the memory allocation for the arguments
void freeArgs(char *args[200], int numOfArgs) {
  int i;
  for (i = 0; i < numOfArgs; i++) {
    free(args[i]);
  }
}

//inital the struct manager
void initialManager(Manager *manager) {
  manager->history_index = 0;
  manager->history_pid_index = 0;
  manager->jobs_index = 0;
  manager->jobs_pid_index = 0;
  strcpy(manager->currentPath, "null");
}

//run the history command in the child process
void excecuteHistory(Manager *manager) {
  char command[20] = "";
  char *args[20] = {NULL};
  int stat;
  pid_t pid;
  int i;
  int length = manager->history_index;
  if ((pid = fork()) == -1) {
    fprintf(stderr, "Error in system call\n");
  } else if (pid != 0) {
    //the father process is responsible to save the pid of the child in the manager
    signal(SIGCHLD, SIG_IGN);
    manager->history_pid[manager->history_pid_index] = pid;
    manager->history_pid_index++;
    waitpid(pid, &stat, 0);
  } else {
    //the child process prints all the commands that user wrote and the id of the process that execute them.
    manager->history_pid[manager->history_pid_index] = getpid();
    manager->history_pid_index++;
    for (i = 0; i < length; i++) {
      if (isProcessAlive(manager->history_pid[i])) {
        printf("%d %s RUNNING\n", manager->history_pid[i], manager->history[i]);
      } else {
        printf("%d %s DONE\n", manager->history_pid[i], manager->history[i]);
      }
    }
    sleep(0.5);
    exit(0);
  }
}

//run the hobs command in the child process
void excecuteJobs(Manager *manager) {
  int stat;
  pid_t pid;
  int i;
  int length = manager->jobs_index;
  if ((pid = fork()) == -1) {
    fprintf(stderr, "Error in system call\n");
  } else if (pid != 0) {
    //the father process is responsible to save the pid of the child in the manager
    signal(SIGCHLD, SIG_IGN);
    manager->history_pid[manager->history_pid_index] = pid;
    manager->history_pid_index++;
    waitpid(pid, &stat, 0);
  } else {
    //the child process prints all the background commands that user wrote and the id of the process that execute them.
    manager->jobs_pid[manager->jobs_pid_index] = getpid();
    manager->jobs_pid_index++;
    for (i = 0; i < length; i++) {
      if (isProcessAlive(manager->jobs_pid[i])) {
        printf("%d %s RUNNING\n", manager->jobs_pid[i], manager->jobs[i]);
      } else {
        printf("%d %s DONE\n", manager->jobs_pid[i], manager->jobs[i]);
      }
    }
    sleep(0.5);
    exit(0);
  }
}

//according to the pid checks if the process is alive/
int isProcessAlive(pid_t pid) {
  if (kill(pid, 0) < 0) {
    if (errno == ESRCH) {
      return 0;
    }
  }
  if (getppid() == pid) {
    return 0;
  }
  return 1;
}

//execute the cd command
int excecuteCd(char *args[200], int numOfArgs, Manager *manager) {
  printf("%d\n", getpid());
  char lastPath[PATH_MAX];
  manager->history_pid[manager->history_pid_index] = getpid();
  manager->history_pid_index++;
  strcpy(lastPath, manager->currentPath);
  //if there is more then one argument
  if (numOfArgs > 2) {
    fprintf(stderr, "Error: Too many arguments\n");
    return 0;
  }
  //the home directory
  if (!(getcwd(manager->currentPath, sizeof(manager->currentPath)) != NULL)) {
    return -1;
  }
  const char *homeDirectory = getenv("HOME");
  if (homeDirectory == NULL) {
    homeDirectory = getpwuid(getuid())->pw_dir;
  }
  //execute according to the argument
  if (numOfArgs == 1) {
    if (chdir(homeDirectory) != 0) {
      return -1;
    }
  } else if (!strcmp(args[1], "~")) {
    if (chdir(homeDirectory) != 0) {
      return -1;
    }
  } else if (!strcmp(args[1], "-")) {
    if (!strcmp(lastPath, "null")) {
      strcpy(manager->currentPath, "null");
      return 0;
    }
    if (chdir(lastPath) != 0)
      return -1;
  } else if (!strcmp(args[1], "..")) {
    if (chdir(lastPath) != 0) {
      return -1;
    }
  } else if (args[1][0] == '~') {
    if (chdir(homeDirectory) != 0) {
      return -1;
    }
    char *temp = args[1] + 2;
    if (strlen(temp) && chdir(temp) != 0) {
      strcpy(lastPath, manager->currentPath);
      fprintf(stderr, "Error: No such file or directory\n");
      if (chdir("..") != 0) {
        return -1;
      }
      return -1;
    }
  } else {
    if (chdir(args[1]) != 0) {
      strcpy(lastPath, manager->currentPath);
      fprintf(stderr, "Error: No such file or directory\n");
      return -1;
    }
  }
  return 0;
}

//free the memory the was allocated to the history and jobs array
void freeManager(Manager *manager) {
  int i, history_length = manager->history_index, jobs_length = manager->jobs_index;
  for (i = 0; i < history_length; i++) {
    free(manager->history[i]);
  }
  for (i = 0; i < jobs_length; i++) {
    free(manager->jobs[i]);
  }
}

//allocate memory to the arguments
int allocateMemory(char *args[200], char *token, int i) {
  int j;
  args[i] = strdup(token);
  //if failed
  if (args[i] == NULL) {
    for (j = 0; j < i; j++) {
      free(args[j]);
    }
    return -1;
  }
  return 1;
}