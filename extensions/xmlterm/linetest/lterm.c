/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is lineterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* lterm.c: Test driver for LINETERM
 * CPP options:
 *   USE_NCURSES:           Enable NCURSES as a screen display option
 *   NCURSES_MOUSE_VERSION: Enable NCURSES mouse operations
 *   LINUX:                 for Linux2.0/glibc
 *   SOLARIS:               for Solaris2.6
 */

#include <stdio.h>
#include <signal.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <assert.h>

#ifdef USE_NCURSES
#include <ncurses.h>
#endif

#ifndef _REENTRANT
#define _REENTRANT
#endif /* !_REENTRANT */

#include <pthread.h>

#ifdef SOLARIS
#include <stropts.h>
#include <poll.h>
#endif

#ifdef LINUX
#include <sys/ioctl.h>
#include <sys/poll.h>
#endif

#include "lineterm.h"
#include "tracelog.h"

#define MAXPROMPT 256          /* Maximum length of prompt regexp */
#define MAXCOL 4096            /* Maximum columns in line buffer */

/* (0,0) is upper lefthand corner of window */

/* Character attributes
        A_NORMAL        Normal display (no highlight)
        A_STANDOUT      Best highlighting mode of the terminal.
        A_UNDERLINE     Underlining
        A_REVERSE       Reverse video
        A_BLINK         Blinking
        A_DIM           Half bright
        A_BOLD          Extra bright or bold
        A_PROTECT       Protected mode
        A_INVIS         Invisible or blank mode
        A_ALTCHARSET    Alternate character set
        A_CHARTEXT      Bit-mask to extract a character
        COLOR_PAIR(n)   Color-pair number n
*/

/* GLOBAL VARIABLES */
static int ncursesFlag = 0;
static int ptyFlag = 1;
static int debugFlag = 0;
static int ltermNumber = -1;
static char *debugFunction;
static char *ttyDevice;
static char *errDevice;

static int screenMode = 0;
static int topScrollRow, botScrollRow;

static struct termios tios;    /* TERMIOS structure */

static void finish(int sig);
static pthread_t output_handler_thread_ID;

static void *output_handler(void *arg);

static void input_handler(int *plterm);

#ifdef USE_NCURSES
static SCREEN  *termScreen = NULL;
#endif

int main(int argc, char *argv[]) {
  FILE *inFile, *outFile;
  UNICHAR uregexp[MAXPROMPT+1];
  int argNo, options, processType, retValue;
  int remaining, decoded;
  int messageLevel;
  char *promptStr;
  char **commandArgs;
  char *defaultCommand[] = {(char *)getenv("SHELL"), "-i", NULL};

  /* Process command line arguments */
  ncursesFlag = 0;
  ptyFlag = 1;
  debugFlag = 0;
  processType = LTERM_DETERMINE_PROCESS;
  debugFunction = NULL;
  ttyDevice = NULL;
  errDevice = NULL;
  promptStr = "#$%>?";  /* JUST A LIST OF DELIMITERS AT PRESENT */

  argNo = 1;
  while (argNo < argc) {

    if ((strcmp(argv[argNo],"-h") == 0)||(strcmp(argv[argNo],"-help") == 0)) {
      fprintf(stderr, "Usage: %s [-help] [-ncurses] [-nopty] [-debug] [-tcsh / -bash] [-function debug_fun] [-tty /dev/ttyname] [-err /dev/ttyname] [-prompt <prompt>] <command> ...\n", argv[0]);
      exit(0);

    } else if (strcmp(argv[argNo],"-ncurses") == 0) {
      ncursesFlag = 1;
      argNo++;

    } else if (strcmp(argv[argNo],"-nopty") == 0) {
      ptyFlag = 0;
      argNo++;

    } else if (strcmp(argv[argNo],"-debug") == 0) {
      debugFlag = 1;
      argNo++;

    } else if (strcmp(argv[argNo],"-bash") == 0) {
      processType = LTERM_BASH_PROCESS;
      argNo++;

    } else if (strcmp(argv[argNo],"-tcsh") == 0) {
      processType = LTERM_TCSH_PROCESS;
      argNo++;

    } else if (strcmp(argv[argNo],"-function") == 0) {
      argNo++;
      if (argNo < argc) {
        debugFunction = argv[argNo++];
      }

    } else if (strcmp(argv[argNo],"-tty") == 0) {
      argNo++;
      if (argNo < argc) {
        ttyDevice = argv[argNo++];
      }

    } else if (strcmp(argv[argNo],"-err") == 0) {
      argNo++;
      if (argNo < argc) {
        errDevice = argv[argNo++];
      }

    } else if (strcmp(argv[argNo],"-prompt") == 0) {
      argNo++;
      if (argNo < argc) {
        promptStr = argv[argNo++];
      }

    } else
      break;
  }

  if (argNo < argc) {
    /* Execute specified command */
    commandArgs = argv + argNo;
  } else {
    /* Execute default shell */
    commandArgs = defaultCommand;
  }

  /* Convert prompt string to Unicode */
  retValue = utf8toucs(promptStr, strlen(promptStr), uregexp, MAXPROMPT,
                       0, &remaining, &decoded);
  if ((retValue < 0) || (remaining > 0)) {
    fprintf(stderr, "lterm: Error in decoding prompt string\n");
    exit(1);
  }

  assert(decoded <= MAXPROMPT);
  uregexp[decoded] = U_NUL;

  if (debugFlag) {
    messageLevel = 98;
  } else {
    messageLevel = 1;
  }

  if (errDevice != NULL) {
    /* Redirect debug STDERR output to specified device */
    int errfd = -1;
    if ( (errfd = open(errDevice, O_WRONLY)) == -1)
        perror("lterm");

    if (dup2(errfd, 2) == -1) {
      fprintf(stderr, "lterm: Failed dup2 for specified stderr\n");
      exit(-1);
    }

    fprintf(stderr, "\n\nlterm: Echoing %s output to %s\n",
            argv[0], errDevice);
  }

  signal(SIGINT, finish); /* Interrupt handler */

  if (ncursesFlag) {
    /* NCURSES mode */

#ifdef USE_NCURSES
    if (ttyDevice == NULL) {
      /* Initialize screen on controlling TTY */
      initscr();

    } else {
      /* Initialize screen on specified TTY */
      if (errDevice != NULL)
        fprintf(stderr, "lterm-00: Opening xterm %s\n", ttyDevice);
      inFile = fopen( ttyDevice, "r");
      outFile = fopen( ttyDevice, "w");
      termScreen = newterm("xterm", outFile, inFile);
      set_term(termScreen);
    }

    /* NCURSES screen settings */
    cbreak();                 /* set terminal to raw (non-canonical) mode */
    noecho();                 /* Disable terminal echo */
    nonl();                   /* Do not translate newline */
    intrflush(stdscr, FALSE); /* Flush input on interrupt */
    keypad(stdscr, TRUE);     /* Enable user keypad */

#ifdef NCURSES_MOUSE_VERSION
    mousemask(BUTTON1_CLICKED, NULL); /* Capture Button1 click events */
#endif

    clear(); /* Clear screen */
#endif  /* USE_NCURSES */

  } else {
    /* XTERM mode */

    /* Get terminal attributes */
    if (tcgetattr(0, &tios) == -1) {
      fprintf(stderr, "lterm: Failed to get TTY attributes\n");
      exit(-1);
    }

    /* Disable signals, canonical mode processing, and echo  */
    tios.c_lflag &= ~(ISIG | ICANON | ECHO );

    /* set MIN=1 and TIME=0 */
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    /* Set terminal attributes */
    if (tcsetattr(0, TCSAFLUSH, &tios) == -1) {
      fprintf(stderr, "lterm: Failed to set TTY attributes\n");
      exit(-1);
    }

  }

  /* Initialize LTERM operations */
  lterm_init(0);

  if (errDevice != NULL) {
    tlog_message("lterm-00: Testing tlog_message\n");
    tlog_warning("lterm-00: Testing tlog_warning\n");
    fprintf(stderr, "lterm-00: ");
    tlog_unichar(uregexp, ucslen(uregexp));
    tlog_set_level(LTERM_TLOG_MODULE, messageLevel, debugFunction);
  }

  if (errDevice != NULL)
    fprintf(stderr, "lintest-00: Opening LTERM to execute %s\n", commandArgs[0]);

  options = 0;
  if (!ptyFlag) options |= LTERM_NOPTY_FLAG;

  ltermNumber = lterm_new();
  retValue = lterm_open(ltermNumber, commandArgs, NULL, NULL, uregexp,
                        options, processType,
                        24, 80, 0, 0,
                        NULL, NULL);
  if (retValue < 0) {
    fprintf(stderr, "lterm: Error %d in opening LTERM\n", retValue);
    exit(1);
  }

  /* Create output handler thread */
  retValue = pthread_create(&output_handler_thread_ID,  NULL,
                            output_handler, (void *) &ltermNumber);
  if (retValue != 0) {
    fprintf(stderr, "lterm: Error %d in creating OUTPUT_HANDLER thread\n",
                     retValue);
    finish(0);
  }

  if (errDevice != NULL)
    fprintf(stderr, "lterm-00: Created OUTPUT_HANDLER thread\n");

  /* Process input */
  input_handler(&ltermNumber);

  /* Join output handler thread */
  if (errDevice != NULL)
    fprintf(stderr, "lterm-00: Joining OUTPUT_HANDLER thread\n");

  retValue = pthread_join(output_handler_thread_ID,  NULL);
  if (retValue != 0) {
    fprintf(stderr, "lterm: Error %d in joining OUTPUT_HANDLER thread\n",
                     retValue);
    finish(0);
  }

  finish(0);
}

void finish(int sig)
{
  if (ncursesFlag) {
#ifdef USE_NCURSES
    endwin(); /* Close window */
    if (termScreen != NULL)
      delscreen(termScreen);
#endif
  }

  if (ltermNumber >= 0) {
    /* Close and delete LTERM */
    lterm_delete(ltermNumber);
  }

  if (errDevice != NULL)
    fprintf(stderr, "finished-00: Finished\n");

  exit(0);
}

/** Output an Unicode message to specified file descriptor or
 * to NCURSES window if fd == -1;
 */
void writeUnicode(int fd, const UNICHAR *buf, int count)
{
  char str[MAXCOL];
  int j, k;

  k = 0;
  for (j=0; j<count; j++) {

    if (k >= MAXCOL-4) {
      if (MAXCOL >= 4) {
        str[MAXCOL-4] = '.';
        str[MAXCOL-3] = '.';
        str[MAXCOL-2] = '.';
      }
      k = MAXCOL-1;
      break;
    }

    /* TEMPORARY IMPLEMENTATION: just truncate Unicode to byte characters */
    str[k++] = buf[j];
  }

  if (k == 0) return;

  if (fd >= 0) {
    if (write(fd, str, k) != k) {
      fprintf(stderr, "writeUnicode: Error in writing to FD %d\n", fd);
      exit(-1);
    }
  } else {
#ifdef USE_NCURSES
    addnstr(str, k);
#endif
  }
}


/** Output an Unicode message to specified output stream
 * if NOCONTROL is true, control characters are converted to printable
 * characters before output
 */
void printUnicode(FILE *outStream, const UNICHAR *buf, int count, int noControl)
{
  char str[MAXCOL];
  int j, k;

  k = 0;
  for (j=0; j<count; j++) {

    if (k >= MAXCOL-4) {
      if (MAXCOL >= 4) {
        str[MAXCOL-4] = '.';
        str[MAXCOL-3] = '.';
        str[MAXCOL-2] = '.';
      }
      k = MAXCOL-1;
      break;
    }

    if (!noControl && ((buf[j] < U_SPACE) || (buf[j] == U_DEL)) ) {
      /* Control character */
      str[k++] = U_CARET;
      str[k++] = buf[j]+U_ATSIGN;
    } else {
      /* Printable character */
      /* TEMPORARY IMPLEMENTATION: just truncate Unicode to byte characters */
      str[k++] = buf[j];
    }
  }

  /* Insert terminating null character and display string */
  str[k++] = '\0';
  fprintf(outStream, "%s\n", str);
}


void input_handler(int *plterm)
{
  char ch;
  UNICHAR uch;
  int n_written;

  for (;;) {
    /* Read a character from TTY (raw mode) */
    if (ncursesFlag) {
#ifdef USE_NCURSES
      ch = getch();
      if (ch == '\r')
        ch = '\n';
#endif
    } else {
      ch = getchar();
    }

    /* fprintf(stderr, "input_handler-00: ch=%d\n", ch); */

    if (ch == 0x1D) {
      fprintf(stderr, "input_handler-00: C-] character read; terminating\n");
      break;
    }

    uch = (UNICHAR) ch;
    n_written = lterm_write(*plterm, &uch, 1, LTERM_WRITE_PLAIN_INPUT);

    /* Exit loop if TTY has been closed */
    if (n_written == -2) {
      if (errDevice != NULL)
        fprintf(stderr, "input_handler-00: pseudo-TTY has been closed\n", *plterm);
      break;
    }

    if (n_written < 0) {
      fprintf(stderr, "input_handler: Error %d return from lterm_write\n",
              n_written);
      return;
    }
  }

  /* Close LTERM */
  if (errDevice != NULL)
    fprintf(stderr, "input_handler-00: Closing LTERM %d\n", *plterm);

  /* Close and delete LTERM */
  lterm_delete(*plterm);
  *plterm = -1;
}

void *output_handler(void *arg)
{
  int *plterm = (int *) arg;
  int timeout = -1;
  UNICHAR buf[MAXCOL];
  UNISTYLE style[MAXCOL];
  int n_read, opcodes, opvals, buf_row, buf_col, cursor_row, cursor_col;
  int xmax, ymax, x, y, c;
#ifdef USE_NCURSES
  MEVENT mev;
#endif

  if (errDevice != NULL)
    fprintf(stderr, "output_handler-00: thread ID = %d, LTERM=%d\n",
                    pthread_self(), *plterm);

  /* Get screen size */
  if (ncursesFlag) {
#ifdef USE_NCURSES
    getmaxyx(stdscr, ymax, xmax);
#endif

  } else {
    ymax = 24;
    xmax = 80;
  }

  if (errDevice != NULL)
    fprintf(stderr, "output_handler-00: screen xmax = %d, ymax = %d\n",
            xmax,ymax);

  for (;;) {
    n_read = lterm_read(*plterm, timeout, buf, MAXCOL,
                        style, &opcodes, &opvals,
                        &buf_row, &buf_col, &cursor_row, &cursor_col);

    if (n_read == -1) {
      fprintf(stderr, "output_handler: Error %d return from lterm_read\n",
                      n_read);
      return NULL;
    }

    /* Exit loop if TTY has been closed;
     * leave it to input handler to close the LTERM.
     */
    if (n_read == -2) {
      if (errDevice != NULL)
        fprintf(stderr, "output_handler: pseudo-TTY has been closed\n");
      break;
    }

    if (debugFlag) {
      fprintf(stderr, "output_handler-00: n_read=%d, opcodes=%x, opvals=%d, buf_row/col=%d/%d, cursor_row/col=%d/%d\n",
            n_read, opcodes, opvals, buf_row, buf_col, cursor_row, cursor_col);
      fprintf(stderr, "output_handler-00: U(%d): ", n_read);
      printUnicode(stderr, buf, n_read, 1);
      fprintf(stderr, "\n");
    }

    if (opcodes & LTERM_STREAMDATA_CODE) {
      /* Stream data */
      if (debugFlag)
        fprintf(stderr, "output_handler-00: STREAMDATA\n");


    } else if (opcodes & LTERM_SCREENDATA_CODE) {
      /* Screen data */

      if (!screenMode) {
        screenMode = 1;
        topScrollRow = ymax-1;
        botScrollRow = 0;
      }

      if (debugFlag)
        fprintf(stderr, "output_handler-00: SCREENDATA, topScrollRow=%d, botScrollRow=%d\n", topScrollRow, botScrollRow);

      if (ncursesFlag) {
        /* NCURSES mode */

#ifdef USE_NCURSES
        if (opcodes & LTERM_CLEAR_CODE) {
          clear();

        } else if (opcodes & LTERM_INSERT_CODE) {
          if ((botScrollRow > 0) && (opvals > 0)) {
            move(ymax-botScrollRow-opvals, 0);
            insdelln(-opvals);
          }

          move(ymax-1-buf_row, 0);
          insdelln(opvals);

        } else if (opcodes & LTERM_DELETE_CODE) {

          move(ymax-1-buf_row, 0);
          insdelln(-opvals);

          if ((botScrollRow > 0) && (opvals > 0)) {
            move(ymax-botScrollRow-opvals, 0);
            insdelln(opvals);
          }

        } else if (opcodes & LTERM_SCROLL_CODE) {
          topScrollRow = opvals;
          botScrollRow = buf_row;

        } else if (opcodes & LTERM_OUTPUT_CODE) {
          /* Data available for display */
          move(ymax-1-buf_row, buf_col);
          clrtoeol();

          if (n_read > 0) {
            if (style[0] != LTERM_STDOUT_STYLE)
              attr_on(A_REVERSE, NULL);

            writeUnicode(-1, buf, n_read);

            if (style[0] != LTERM_STDOUT_STYLE)
              attr_off(A_REVERSE, NULL);
          }
        }

        /* Position cursor */
        move(ymax-1-cursor_row, cursor_col);

        refresh();
#endif  /* USE_NCURSES */

      } else {
        /* XTERM MODE */
        char esc_seq[21];

        if (opcodes & LTERM_CLEAR_CODE) {
          /* Clear screen */
          write(1, "\033[H\033[2J", 7);

        } else if (opcodes & LTERM_INSERT_CODE) {
          /* Insert lines */
          sprintf(esc_seq, "\033[%d;%dH", ymax-buf_row, buf_col+1);
          write(1, esc_seq, strlen(esc_seq));

          sprintf(esc_seq, "\033[%dL", opvals);
          write(1, esc_seq, strlen(esc_seq));

        } else if (opcodes & LTERM_DELETE_CODE) {
          /* Delete lines */
          sprintf(esc_seq, "\033[%d;%dH", ymax-buf_row, buf_col+1);
          write(1, esc_seq, strlen(esc_seq));

          sprintf(esc_seq, "\033[%dM", opvals);
          write(1, esc_seq, strlen(esc_seq));

        } else if (opcodes & LTERM_SCROLL_CODE) {
          /* Set scrolling region */
          sprintf(esc_seq, "\033[%d;%dr", ymax-opvals, ymax-buf_row);
          write(1, esc_seq, strlen(esc_seq));

        } else if (opcodes & LTERM_OUTPUT_CODE) {
          /* Data available for display */
          sprintf(esc_seq, "\033[%d;%dH", ymax-buf_row, buf_col+1);
          write(1, esc_seq, strlen(esc_seq));

          if (n_read > 0) {
            if (style[0] != LTERM_STDOUT_STYLE)
              write(1, "\033[7m", 4);

            writeUnicode(1, buf, n_read);

            if (style[0] != LTERM_STDOUT_STYLE)
              write(1, "\033[27m", 5);
          }
        }

        sprintf(esc_seq, "\033[%d;%dH", ymax-cursor_row, cursor_col+1);
        write(1, esc_seq, strlen(esc_seq));
      }

    } else if (opcodes & LTERM_LINEDATA_CODE) {
      /* Line data */
      if (debugFlag)
        fprintf(stderr, "output_handler-00: LINEDATA\n");

      if (screenMode)
        screenMode = 0;

      if (ncursesFlag) {
        /* NCURSES mode */

#ifdef USE_NCURSES
        /* Move cursor to bottom of screen, clear line and display line */
        move(ymax-1,0);
        clrtoeol();
        if (n_read > 0)
          writeUnicode(-1, buf, n_read);
        move(ymax-1,cursor_col);

        if (opcodes & LTERM_NEWLINE_CODE) {
          move(0,0);
          insdelln(-1);
          move(ymax-1,0);
        }

        refresh();
#endif  /* USE_NCURSES */

      } else {
        /* Screen mode */
        int j;

        /* Clear line */
        write(1, "\033[2K", 4);
        write(1, "\r", 1);

        if (opcodes & LTERM_META_CODE)
          write(1, "META", 4);

        if (n_read > 0)
          writeUnicode(1, buf, n_read);

        for (j=0; j< (n_read-cursor_col); j++)
          write(1, "\033[D", 3);

        if (opcodes & LTERM_BELL_CODE)
          write(1, "\007", 1);

        if (opcodes & LTERM_CLEAR_CODE)
          write(1, "\033[H\033[2J", 7);

        if (opcodes & LTERM_NEWLINE_CODE)
          write(1, "\n", 1);
      }

    } else if (opcodes != 0) {
      fprintf(stderr, "output_handler: invalid opcodes %x\n", opcodes);
    }
  }

  return NULL;
}
