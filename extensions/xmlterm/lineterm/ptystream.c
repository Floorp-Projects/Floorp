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

/* ptystream.c: pseudo-TTY stream implementation
 * CPP options:
 *   LINUX:         for Linux2.0/glibc
 *   SOLARIS:       for Solaris2.6
 *   BSDFAMILY:     for FreeBSD, ...
 *   NOERRMSG:      for suppressing all error messages
 *   USE_NSPR_BASE: use NSPR to log error messages (defines NOERRMSG as well)
 *   DEBUG_LTERM:   for printing some debugging output to STDERR
 */

/* system header files */


#ifdef LINUX
#define _BSD_SOURCE  1
#endif

#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#if defined(LINUX) || defined(BSDFAMILY) || defined(HPUX11)
#include <sys/ioctl.h>
#endif

#include <unistd.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef USE_NSPR_BASE
#include "prlog.h"
#define NOERRMSG 1
#endif

/* public declarations */
#include "ptystream.h"

/* private declarations */
static int openPTY(struct ptys *ptyp, int noblock);
static int attachToTTY(struct ptys *ptyp, int errfd, int noecho);
static int setTTYAttr(int ttyFD, int noecho);
static void pty_error(const char *errmsg, const char *errmsg2);

/* parameters */
#define C_CTL_C	  '\003'     /* ^C */
#define C_CTL_D   '\004'     /* ^D */
#define C_CTL_H	  '\010'     /* ^H */
#define C_CTL_O	  '\017'     /* ^O */
#define C_CTL_Q	  '\021'     /* ^Q */
#define C_CTL_R	  '\022'     /* ^R */
#define C_CTL_S	  '\023'     /* ^S */
#define C_CTL_U	  '\025'     /* ^U */
#define C_CTL_V	  '\026'     /* ^V */
#define C_CTL_W   '\027'     /* ^W */
#define C_CTL_Y	  '\031'     /* ^Y */
#define C_CTL_Z	  '\032'     /* ^Z */
#define C_CTL_BSL '\034'     /* ^\ */

/* Disable special character functions */
#ifdef _POSIX_VDISABLE
#define VDISABLE	_POSIX_VDISABLE
#else
#define VDISABLE	255
#endif

#define	PTYCHAR1    "pqrstuvwxyzPQRSTUVWXYZ"
#define	PTYCHAR2    "0123456789abcdef"


/* creates a new pseudo-TTY */
int pty_create(struct ptys *ptyp, char *const argv[],
               int rows, int cols, int x_pixels, int y_pixels,
               int errfd, int noblock, int noecho, int noexport, int debug)
{
  pid_t child_pid;
  int errfd2;

  if (!ptyp) {
    pty_error("pty_create: NULL value for PTY structure", NULL);
    return -1;
  }

  /* Set debug flag */
  ptyp->debug = debug;

#ifdef DEBUG_LTERM
  if (ptyp->debug)
    fprintf(stderr, "00-pty_create: errfd=%d, noblock=%d, noecho=%d, noexport=%d\n",
                     errfd, noblock, noecho, noexport);
#endif

  /* Open PTY */
  if (openPTY(ptyp, noblock) == -1) return -1;

#ifndef BSDFAMILY
  /* Set default TTY size */
  if (pty_resize(ptyp, rows, cols, x_pixels, y_pixels) != 0)
    return -1;
#endif

  if (errfd >= -1) {
    /* No STDERR pipe */
    ptyp->errpipeFD = -1;
    errfd2 = errfd;

  } else {
    /* Create pipe to handle STDERR output */
    int pipeFD[2];

    if (pipe(pipeFD) == -1) {
      pty_error("pty_create: STDERR pipe creation failed", NULL);
      return -1;
    }

    /* Copy pipe file descriptors */
    ptyp->errpipeFD = pipeFD[0];
    errfd2          = pipeFD[1];
  }

  /* Fork a child process (VFORK) */
  child_pid = vfork();
  if (child_pid < 0) {
    pty_error("pty_create: vfork failed", NULL);
    return -1;
  }

  ptyp->pid = child_pid;

#ifdef DEBUG_LTERM
  if (ptyp->debug)
    fprintf(stderr, "00-pty_create: Fork child pid = %d, initially attached to %s\n",
                     child_pid, ttyname(0));
#endif

  if (child_pid == 0) {
    /* Child process */

    /* Attach child to slave TTY */
    if (attachToTTY(ptyp, errfd2, noecho) == -1) return -1;

#ifdef BSDFAMILY
    /* Set default TTY size */
    if (pty_resize(NULL, rows, cols, x_pixels, y_pixels) != 0)
      return -1;
#endif

    /* Set default signal handling */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    /* Set ignore signal handling */
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    if (argv != NULL) {
      /* Execute specified command with arguments */
      if (noexport)
        execve(argv[0], argv, NULL);
      else
        execvp(argv[0], argv);

      pty_error("Error in executing command ", argv[0]);
      return -1;

    } else {
      /* Execute $SHELL or /bin/sh by default */
      char *shell = (char *) getenv("SHELL");

      if ((shell == NULL) || (*shell == '\0'))
        shell = "/bin/sh";

      if (noexport)
        execle(shell, shell, NULL, NULL);
      else
        execlp(shell, shell, NULL);

      pty_error("pty_create: Error in executing command ", shell);
      return -1;
    }

  }

  if (errfd < -1) {
    /* Close write end of STDERR pipe in parent process */
    close(errfd2);
  }

  /* Return from parent */
  return 0;
}


/* closes a pseudo-TTY */
int pty_close(struct ptys *ptyp)
{
  if (!ptyp) {
    pty_error("pty_close: NULL value for PTY structure", NULL);
    return -1;
  }

  kill(ptyp->pid, SIGKILL);
  ptyp->pid = 0;

  close(ptyp->ptyFD);
  ptyp->ptyFD = -1;

  if (ptyp->errpipeFD >= 0) {
    close(ptyp->errpipeFD);
    ptyp->errpipeFD = -1;
  }

  return 0;
}


/* resizes a PTY; if ptyp is null, resizes file desciptor 0,
 * returning 0 on success and -1 on error.
 */
int pty_resize(struct ptys *ptyp, int rows, int cols,
                                  int xpix, int ypix)
{
  struct winsize wsize;
  int fd = ptyp ? ptyp->ptyFD : 0;

  /* Set TTY window size */
  wsize.ws_row = (unsigned short) rows;
  wsize.ws_col = (unsigned short) cols;
  wsize.ws_xpixel = (unsigned short) xpix;
  wsize.ws_ypixel = (unsigned short) ypix;

  if (ioctl(fd, TIOCSWINSZ, &wsize ) == -1) {
    pty_error("pty_resize: Failed to set TTY window size", NULL);
    return -1;
  }

  return 0;
}


static int openPTY(struct ptys *ptyp, int noblock)
{
  char ptyName[PTYNAMELEN+1], ttyName[PTYNAMELEN+1];

  int plen, tlen, ptyFD, letIndex, devIndex;

  (void) strncpy(ptyName, "/dev/pty??", PTYNAMELEN+1);
  (void) strncpy(ttyName, "/dev/tty??", PTYNAMELEN+1);

  plen = strlen(ptyName);
  tlen = strlen(ttyName);

  assert(ptyp != NULL);
  assert(plen <= PTYNAMELEN);
  assert(tlen <= PTYNAMELEN);

  ptyFD = -1;
  letIndex = 0;
  while (PTYCHAR1[letIndex] && (ptyFD == -1)) {
    ttyName[tlen - 2] =
      ptyName[plen - 2] = PTYCHAR1 [letIndex];

    devIndex = 0;
    while (PTYCHAR2[devIndex] && (ptyFD == -1)) {
      ttyName [tlen - 1] =
        ptyName [plen - 1] = PTYCHAR2 [devIndex];

      if ((ptyFD = open(ptyName, O_RDWR)) >= 0) {
        if (access(ttyName, R_OK | W_OK) != 0) {
          close(ptyFD);
          ptyFD = -1;
        }
      }
      devIndex++;
    }

    letIndex++;
  }

  if (ptyFD == -1) {
    pty_error("openPTY: Unable to open pseudo-tty", NULL);
    return -1;
  }

  if (noblock) {
    /* Set non-blocking mode */
    fcntl(ptyFD, F_SETFL, O_NDELAY);
  }

  strncpy(ptyp->ptydev, ptyName, PTYNAMELEN+1);
  strncpy(ptyp->ttydev, ttyName, PTYNAMELEN+1);

  ptyp->ptyFD = ptyFD;

#ifdef DEBUG_LTERM
  if (ptyp->debug)
    fprintf(stderr, "00-openPTY: Opened pty %s on fd %d\n", ptyName, ptyFD);
#endif

  return 0;
}


/* attaches new process to slave TTY */
static int attachToTTY(struct ptys *ptyp, int errfd, int noecho)
{
  int          ttyFD, fd, fdMax;
  pid_t        sid;
  gid_t        gid;
  unsigned int ttyMode = 0622;

  assert(ptyp != NULL);

  /* Create new session */
  sid = setsid();

#ifdef DEBUG_LTERM
  if (ptyp->debug)
    fprintf(stderr, "00-attachToTTY: Returned %d from setsid\n", sid);
#endif

  if (sid < 0) {
#ifndef NOERRMSG
    perror("attachToTTY");
#endif
    return -1;
  }

  if ((ttyFD = open(ptyp->ttydev, O_RDWR)) < 0) {
    pty_error("attachToTTY: Unable to open slave tty ", ptyp->ttydev );
    return -1;
  }

#ifdef DEBUG_LTERM
  if (ptyp->debug)
    fprintf(stderr,"00-attachToTTY: Attaching process %d to TTY %s on fd %d\n",
                    getpid(), ptyp->ttydev, ttyFD);
#endif

  /* Change TTY ownership and permissions*/
  gid = getgid();

  fchown(ttyFD, getuid(), gid);
  fchmod(ttyFD, ttyMode);

  /* Set TTY attributes (this actually seems to be harmful; so commented out!)
   */
  /* if (setTTYAttr(ttyFD, noecho) == -1) return -1; */

  /* Redirect to specified descriptor or to PTY */
  if (errfd >= 0) {
    /* Redirect STDERR to specified file descriptor */
    if (dup2(errfd, 2) == -1) {
      pty_error("attachToTTY: Failed dup2 for specified stderr", NULL);
      return -1;
    }

  } else {
    /* Redirect STDERR to PTY */
    if (dup2(ttyFD, 2) == -1) {
      pty_error("attachToTTY: Failed dup2 for default stderr", NULL);
      return -1;
    }
  }

  /* Redirect STDIN and STDOUT to PTY */
  if (dup2(ttyFD, 0) == -1) {
    pty_error("attachToTTY: Failed dup2 for stdin", NULL);
    return -1;
  }

  if (dup2(ttyFD, 1) == -1) {
    pty_error("attachToTTY: Failed dup2 for stdout", NULL);
    return -1;
  }

  /* Close all other file descriptors in child process */
  fdMax = sysconf(_SC_OPEN_MAX);
  for (fd = 3; fd < fdMax; fd++)
    close(fd);

#ifdef BSDFAMILY
  ioctl(0, TIOCSCTTY, 0);
#endif

  /* Set process group */
  tcsetpgrp(0, sid);

  /* close(open(ptyp->ttydev, O_RDWR, 0)); */

  return 0;
}


/* sets slave TTY attributes (NOT USED) */
static int setTTYAttr(int ttyFD, int noecho)
{
  struct termios tios;

  /* Get TTY attributes */
  if (tcgetattr(ttyFD, &tios ) == -1) {
#ifndef NOERRMSG
    perror("setTTYAttr");
#endif
    pty_error("setTTYattr: Failed to get TTY attributes", NULL);
    return -1;
  }
  memset(&tios, 0, sizeof(struct termios));

  /* TERMIOS settings for TTY */
  tios.c_cc[VINTR]    = C_CTL_C;
  tios.c_cc[VQUIT]    = C_CTL_BSL;
  tios.c_cc[VERASE]   = C_CTL_H;
  tios.c_cc[VKILL]    = C_CTL_U;
  tios.c_cc[VEOF]     = C_CTL_D;
  tios.c_cc[VEOL]     = VDISABLE;
  tios.c_cc[VEOL2]    = VDISABLE;
#ifdef SOLARIS
  tios.c_cc[VSWTCH]   = VDISABLE;
#endif
  tios.c_cc[VSTART]   = C_CTL_Q;
  tios.c_cc[VSTOP]    = C_CTL_S;
  tios.c_cc[VSUSP]    = C_CTL_Z;
#ifndef HPUX11
  tios.c_cc[VREPRINT] = C_CTL_R;
  tios.c_cc[VDISCARD] = C_CTL_O;
#endif /* !HPUX11 */
  tios.c_cc[VWERASE]  = C_CTL_W;
  tios.c_cc[VLNEXT]   = C_CTL_V;

  tios.c_cc[VMIN]     = 1;    /* Wait for at least 1 char of input */
  tios.c_cc[VTIME]    = 0;    /* Wait indefinitely (block)  */

#ifdef BSDFAMILY
  tios.c_iflag = (BRKINT | IGNPAR | ICRNL | IXON
                  | IMAXBEL);
  tios.c_oflag = (OPOST | ONLCR);

#else  /* !BSDFAMILY */
  /* Input modes */
  tios.c_iflag &= ~IUCLC;     /* Disable map of upper case input to lower*/
  tios.c_iflag &= ~IGNBRK;    /* Do not ignore break */
  tios.c_iflag &= ~BRKINT;    /* Do not signal interrupt on break either */

  /* Output modes */
  tios.c_oflag &= ~OPOST;     /* Disable output postprocessing */
  tios.c_oflag &= ~ONLCR;     /* Disable mapping of NL to CR-NL on output */
  tios.c_oflag &= ~OLCUC;     /* Disable map of lower case output to upper */
                              /* No output delays */
  tios.c_oflag &= ~(NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);

#endif  /* !BSDFAMILY */

  /* control modes */
  tios.c_cflag |= (CS8 | CREAD);

  /* line discipline modes */
  if (noecho)
    tios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOKE | ECHONL | ECHOPRT |
                      ECHOCTL); /* Disable echo */
  else
    tios.c_lflag |=  (ECHO | ECHOE | ECHOK | ECHOKE | ECHONL | ECHOPRT |
                      ECHOCTL); /* Enable echo */

  tios.c_lflag |= ISIG;       /* Enable signals */
  tios.c_lflag |= ICANON;     /* Enable erase/kill and eof processing */

  /* NOTE: tcsh does not echo to be turned off if TERM=xterm;
           setting TERM=dumb allows echo to be turned off,
           but command completion is turned off as well */

  /* Set TTY attributes */
  cfsetospeed (&tios, B38400);
  cfsetispeed (&tios, B38400);

  if (tcsetattr(ttyFD, TCSADRAIN, &tios ) == -1) {
#ifndef NOERRMSG
    perror("setTTYAttr");
#endif
    pty_error("setTTYattr: Failed to set TTY attributes", NULL);
    return -1;
  }

  return 0;
}


/* displays an error message, optionally concatenated with another */
static void pty_error(const char *errmsg, const char *errmsg2) {

#ifndef NOERRMSG
  if (errmsg != NULL) {
    if (errmsg2 != NULL) {
      fprintf(stderr, "%s%s\n", errmsg, errmsg2);
    } else {
      fprintf(stderr, "%s\n", errmsg);
    }
  }
#else
#ifdef USE_NSPR_BASE
  if (errmsg != NULL) {
    if (errmsg2 != NULL) {
      PR_LogPrint("%s%s\n", errmsg, errmsg2);
    } else {
      PR_LogPrint("%s\n", errmsg);
    }
  }
#endif
#endif

}
