/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is lineterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* ptytest.c: Test driver for ptystream.c
 * CPP options:
 *   LINUX:   for Linux2.0/glibc
 *   SOLARIS: for Solaris2.6
 */

#include <stdio.h>
#include <termios.h>

#include <signal.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef SOLARIS
#include <stropts.h>
#include <poll.h>
#endif

#if defined(LINUX) || defined(BSDFAMILY)
#include <sys/ioctl.h>
#include <sys/poll.h>
#endif

#include "ptystream.h"

static struct termios tios;    /* TERMIOS structure */

void pipeTest(int argc, char *argv[]);
void ptyTest(int argc, char *argv[]);

int echofd = -1;

int main(int argc, char *argv[]) {

  if (argc < 4) {
    fprintf(stderr, "Usage: %s pty|pipe <echo-tty-name>|'' <shell-path> ...\n",
                    argv[0]);
    exit(-1);
  }

  /* Get terminal attributes */
  if (tcgetattr(0, &tios) == -1) {
    fprintf(stderr, "Failed to get TTY attributes\n");
    exit(-1);
  }

  /* Disable signals, canonical mode processing, and echo  */
  tios.c_lflag &= ~(ISIG | ICANON | ECHO );

  /* set MIN=1 and TIME=0 */
  tios.c_cc[VMIN] = 1;
  tios.c_cc[VTIME] = 0;

  /* Set terminal attributes */
  if (tcsetattr(0, TCSAFLUSH, &tios) == -1) {
    fprintf(stderr, "Failed to set TTY attributes\n");
    exit(-1);
  }

  if (*argv[2]) {
    /* Open TTY for echoing */
    if ( (echofd = open(argv[2], O_WRONLY)) == -1)
      perror("ptytest");
    fprintf(stderr, "Echoing %s output to %s\n", argv[1], argv[2]);
    write( echofd, "Echoing PTYTEST output ...\n", 27);
  }

  fprintf(stderr, "Type Control-] to terminate input\n");

  if (strcmp(argv[1],"pipe") == 0) {
    pipeTest(argc-3, argv+3);
  } else {
    ptyTest(argc-3, argv+3);
  }

  if (echofd >= 0) close(echofd);
  exit(0);

}


/* sends raw terminal input/output to a shell through a pseudo-TTY */
void ptyTest(int argc, char *argv[])
{
  struct pollfd pollFD[2];
  nfds_t nfds = 2;
  struct ptys ptyStruc;

  unsigned char ch;
  int ptyFD, pollCode;
  ssize_t n_read, n_written;

  char temstr[3] = "^@";

  /* Create a PTY */
  if (pty_create(&ptyStruc,argv,24,80,0,0,
                 -1,0,0,0,1) == -1) {
    fprintf(stderr, "PTY creation failed\n");
    exit(-1);
  }

  ptyFD = ptyStruc.ptyFD;

  /* fprintf(stderr, "Polling for input on fd=0 (%s) and fd=%d\n",
     ttyname(0), ptyFD ); */

  /* Set up polling to read from parent STDIN and child STDOUT */
  pollFD[0].fd = 0;
  pollFD[0].events = POLLIN;
  pollFD[1].fd = ptyFD;
  pollFD[1].events = POLLIN;

  while ( (pollCode=poll(pollFD,nfds,-1)) >= 0) {
    if (pollCode == 0) continue;
    pollCode = 0;

    if (pollFD[0].revents != 0) {
      /* Read character from parent STDIN and write to child STDIN */
      ch = getchar();

      /* Exit poll loop if a Control-] character is read */
      if (ch == 0x1D) {
        fprintf(stderr, "EXIT ptytest\n");
        break;
      }

      if (write(ptyFD, &ch, 1) != 1) {
        fprintf(stderr, "Error in writing to child STDIN\n");
        exit(-1);
      }

    }

    if (pollFD[1].revents != 0) {
      /* Read character from child STDOUT and write to parent STDOUT */

      if ( (n_read=read(ptyFD, &ch, 1)) < 0) {
        fprintf(stderr, "Error in reading from child STDOUT\n");
        if (echofd >= 0) close(echofd);
        exit(-1);
      }

      if (n_read == 0) { /* End of file */
        if (echofd >= 0) close(echofd);
        exit(0);
      }

      if (echofd >= 0) {
        /* Echo output to another TTY */
        if (ch == 0x7F) {
          write(echofd, "\\DEL", 4);
        } else if (ch == 0x1B) {
          write(echofd, "\\ESC", 4);
        } else if (ch < 0x20) {
          temstr[1]= ch+'@';
          write(echofd, temstr, 2);
        } else {
          write(echofd, &ch, 1);
        }
      }

      if (write(1, &ch, 1) != 1) {
        fprintf(stderr, "Error in writing to parent STDOUT\n");
        exit(-1);
      }

      /*
      if (ioctl(1, I_FLUSH, FLUSHRW) == -1) {
        fprintf(stderr, "Error return from ioctl\n");
        exit(-1);
      }
      */

    }
  }

  if (pollCode != 0) {
    fprintf(stderr, "Error return from poll\n");
    exit(-1);
  }

  /* Close PTY */
  pty_close(&ptyStruc);

}


/* sends raw terminal input/output to a shell through a pipe */
void pipeTest(int argc, char *argv[])
{
  struct pollfd pollFD[2];
  nfds_t nfds = 2;
  pid_t child_pid = -1;   /* child process id */

  unsigned char ch;
  int pipeFD[2], pipeIN, pipeOUT, procIN, procOUT;
  int pollCode;
  ssize_t n_read, n_written;

  char temstr[3] = "^@";

  /* Create process input pipe (assumed unidirectional) */
  if (pipe(pipeFD) == -1) {
    fprintf(stderr, "Input pipe creation failed\n");
    exit(-1);
  }
#ifdef DEBUG
  fprintf(stderr,"Created input pipe: %d %d\n", pipeFD[0], pipeFD[1]);
#endif
  procIN = pipeFD[0];
  pipeIN = pipeFD[1];

  /* Create process output pipe (assumed unidirectional) */
  if (pipe(pipeFD) == -1) {
    fprintf(stderr, "Output pipe creation failed\n");
    exit(-1);
  }
#ifdef DEBUG
  fprintf(stderr,"Created output pipe: %d %d\n", pipeFD[0], pipeFD[1]);
#endif
  pipeOUT = pipeFD[0];
  procOUT = pipeFD[1];

  /* Fork a child process */
  child_pid = fork();
  if (child_pid < 0) {
    fprintf(stderr, "Fork failed\n");
    exit(-1);
  }

#ifdef DEBUG
  fprintf(stderr, "Fork child process %d\n", child_pid);
#endif

  if (child_pid == 0) {
    /* Child process */

    if (dup2(procIN, 0) == -1) {
      fprintf(stderr, "Dup2 failed for stdin of child\n");
      exit(-1);
    }

    if (dup2(procOUT, 1) == -1) {
      fprintf(stderr, "Dup2 failed for stdout of child\n");
      exit(-1);
    }

    execvp(argv[0], argv);
    fprintf(stderr, "Exec failed for command %s\n", argv[0]);
    exit(-1);
  }

  /* Set up polling to read from parent STDIN and child STDOUT */
  pollFD[0].fd = 0;
  pollFD[0].events = POLLIN;
  pollFD[1].fd = pipeOUT;
  pollFD[1].events = POLLIN;

  while ( (pollCode=poll(pollFD,nfds,-1)) >= 0) {

    if (pollFD[0].revents != 0) {
      /* Read character from parent STDIN and write to child STDIN */
      ch = getchar();

      /* Exit poll loop if a Control-] is read */
      if (ch == 0x1D) break;

      if (write(pipeIN, &ch, 1) != 1) {
        fprintf(stderr, "Error in writing to child STDIN\n");
        exit(-1);
      }

    }

    if (pollFD[1].revents != 0) {
      /* Read character from child STDOUT and write to parent STDOUT */

      if ( (n_read=read(pipeOUT, &ch, 1)) < 0) {
        fprintf(stderr, "Error in reading from child STDOUT\n");
        if (echofd >= 0) close(echofd);
        exit(-1);
      }

      if (n_read == 0) { /* End of file */
        if (echofd >= 0) close(echofd);
        exit(0);
      }

      if (echofd >= 0) {
        /* Echo output to another TTY */
        if (ch == 0x7F) {
          write(echofd, "\\DEL", 4);
        } else if (ch == 0x1B) {
          write(echofd, "\\ESC", 4);
        } else if (ch < 0x20) {
          temstr[1]= ch+'@';
          write(echofd, temstr, 2);
        } else {
          write(echofd, &ch, 1);
        }
      }

      if (write(1, &ch, 1) != 1) {
        fprintf(stderr, "Error in writing to parent STDOUT\n");
        exit(-1);
      }

      /*
      if (ioctl(1, I_FLUSH, FLUSHRW) == -1) {
        fprintf(stderr, "Error return from ioctl\n");
        exit(-1);
      }
      */

    }

  }

  if (kill(child_pid, SIGKILL) == -1) {
    fprintf(stderr, "Error return from kill\n");
    exit(-1);
  }

  if (pollCode != 0) {
    fprintf(stderr, "Error return from poll\n");
    exit(-1);
  }

  /* Close pipes (assumed unidirectional) */
  close(pipeIN);
  close(pipeOUT);
  close(procIN);
  close(procOUT);

}
