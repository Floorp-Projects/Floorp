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

/* ltermPrivate.h: Line terminal (LTERM) private header file
 * LTERM provides a "stream" interface to an XTERM-like terminal,
 * using line input/output.
 * CPP options:
 *   LINUX:          for Linux2.0/glibc
 *   SOLARIS:        for Solaris2.6
 *   BSDFAMILY:      for FreeBSD, ...
 *   DEBUG_LTERM:          to enable some debugging output
 *   NO_PTY:         force use of pipes rather than PTY for process
 *                   communication
 *   MOZILLA_CLIENT: set if embedded in Mozilla
 *   USE_NSPR_BASE:  use basic NSPR API (excluding I/O and process creation)
 *   USE_NSPR_LOCK:  use NSPR lock API instead of Unix mutex API
 *   USE_NSPR_IO:    use NSPR I/O and process API instead of Unix API
 *             (current implementation of pseudo-TTY is incompatible with
 *              the NSPR I/O API; set the USE_NSPR_IO option only on platforms
 *              where PTYSTREAM is not implemented)
 */

#ifndef _LTERMPRIVATE_H

#define _LTERMPRIVATE_H 1

/* Standard C header files */
#include <string.h>

/* public declarations */
#include "lineterm.h"
#include "tracelog.h"

/* private declarations */

/* Force use of basic NSPR API if MOZILLA_CLIENT or USE_NSPR_IO are defined */
#if defined(MOZILLA_CLIENT) || defined(USE_NSPR_IO)
#define USE_NSPR_BASE 1
#endif

/* pseudo-TTY stream interface */
#include "ptystream.h"

#define LTERM_ERROR                           TLOG_ERROR
#define LTERM_WARNING                         TLOG_WARNING
#define LTERM_LOG(procname,level,args)        TLOG_PRINT(LTERM_TLOG_MODULE,procname,level,args)
#define LTERM_LOGUNICODE(procname,level,args) TLOG_UNICHAR(LTERM_TLOG_MODULE,procname,level,args)

#ifdef USE_NSPR_BASE           /* Use basic NSPR API (excluding I/O) */

#include "nspr.h"

#define assert       PR_ASSERT

#define int32        PRInt32

#define getenv       PR_GetEnv

#define MALLOC(x)    PR_Malloc(x)
#define REALLOC(x,y) PR_Realloc((x),(y))
#define CALLOC(x,y)  PR_Calloc((x),(y))
#define FREE(x)      PR_Free(x)

#else   /* not USE_NSPR_BASE */

#include <assert.h>

#define int32        long

#define MALLOC(x)    malloc(x)
#define REALLOC(x,y) realloc((x),(y))
#define CALLOC(x,y)  calloc((x),(y))
#define FREE(x)      free(x)

#endif  /* not USE_NSPR_BASE */


#ifdef USE_NSPR_IO           /* Use NSPR I/O API (no PTY implementation) */

typedef PRFileDesc  *FILEDESC;
#define NULL_FILEDESC     0
#define VALID_FILEDESC(x) (x != 0)

typedef PRFileDesc FILESTREAM;

#define SIZE_T   PRInt32
#define WRITE    PR_Write
#define READ     PR_Read
#define CLOSE(x) (PR_Close(x) != PR_SUCCESS)
#define PIPE(x)  (PR_CreatePipe((x),(x)+1) != PR_SUCCESS)

#define POLL(x,y,z)  PR_Poll((x),(y),(z))
#define POLLFD       PRPollDesc
#define POLL_EVENTS  in_flags
#define POLL_REVENTS out_flags
#define POLL_READ    PR_POLL_READ

#define PROCESS           PRProcess *
#define NULL_PROCESS      0
#define VALID_PROCESS(x)  (x != 0)

#else   /* not USE_NSPR_IO */

/* system header files */
#include <unistd.h>
#include <signal.h>

/* Diagnostic/debugging files */
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

#if defined(SOLARIS)
#include <poll.h>
#else
#include <sys/poll.h>
#endif

typedef int FILEDESC;
#define NULL_FILEDESC     -1
#define VALID_FILEDESC(x) (x >= 0)

typedef FILE FILESTREAM;

#define SIZE_T   size_t
#define WRITE    write
#define READ     read
#define CLOSE(x) close(x)
#define PIPE(x)  pipe(x)

#define POLL(x,y,z)  poll((x),(y),(z))
#define POLLFD       pollfd
#define POLL_EVENTS  events
#define POLL_REVENTS revents
#define POLL_READ    POLLIN

#define PROCESS          long
#define NULL_PROCESS     0
#define VALID_PROCESS(x) (x > 0)

#endif  /* not USE_NSPR_IO */


#ifdef USE_NSPR_LOCK           /* Use NSPR lock API */

#define MUTEX_DECLARE(x)     PRLock *x
#define MUTEX_INITIALIZE(x)  ((x=PR_NewLock()) == NULL)
#define MUTEX_LOCK(x)        PR_Lock(x)
#define MUTEX_UNLOCK(x)      PR_Unlock(x)
#define MUTEX_DESTROY(x)     PR_DestroyLock(x)

#define THREAD_DECLARE(x)    PRThread *x
#define THREAD_CREATE        ((x=PR_CreateThread(PR_USER_THREAD, (y), (z), \
                                  PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,    \
                                  PR_JOINABLE_THREAD, 0)) == NULL)
#define THREAD_SELF          PR_GetCurrentThread
#define THREAD_EQUAL(x,y)    ((x) == (y))
#define THREAD_JOIN(x)       (PR_JoinThread(x) != PR_SUCCESS)

#else   /* not USE_NSPR_LOCK */

#include <pthread.h>

#define MUTEX_DECLARE(x)     pthread_mutex_t x
#define MUTEX_INITIALIZE(x)  pthread_mutex_init(&(x), NULL)
#define MUTEX_LOCK(x)        pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)      pthread_mutex_unlock(&(x))
#define MUTEX_DESTROY(x)     pthread_mutex_destroy(&(x))

#define THREAD_DECLARE(x)    pthread_t x
#define THREAD_CREATE(x,y,z) pthread_create(&(x),NULL,(y),(z))
#define THREAD_SELF          pthread_self
#define THREAD_EQUAL(x,y)    pthread_equal(x,y)
#define THREAD_JOIN(x)       pthread_join((x),NULL)

#endif  /* not USE_NSPR_LOCK */


#define MAXTERM 256            /* Maximum number of LTERMs;
                                * affects static memory "footprint"
                                */

#define MAXCOL 4096            /* Maximum columns in line buffer;
                                * affects static memory "footprint";
                                * the only limitation on this value is that
                                * it must fit into a UNICHAR, because of the
                                * way lterm_write and ltermWrite implement
                                * the input buffer pipe.
                                */

#define MAXROW 1024            /* Maximum rows in screen;
                                * primarily affects dynamically allocated
                                * memory
                                */

/* The only obvious limitation on the following is that they should be
 * significantly less than MAXCOL
 */
#define MAXPROMPT 256          /* Maximum length of prompt regexp+1 */
#define MAXRAWINCOMPLETE 5     /* Maximum incomplete raw buffer size */
#define MAXSTREAMTERM 11       /* Maximum stream terminator buffer size */
#define MAXCOOKIESTR 64        /* Maximum length of cookie string+1 */
#define MAXESCAPEPARAMS 16     /* Maximum no. of numeric ESCAPE parameters */
#define MAXSTRINGPARAM  512    /* Maximum length of string ESCAPE parameters */
#define MAXSHELLINITCMD   2    /* Maximum no. of shell init commands */
#define MAXSHELLINITSTR 256    /* Maximum length of shell init string+1 */

#define MAXPTYIN 128           /* 1/2 POSIX minimum MAX_INPUT for PTY */

#define MAXCOLM1 (MAXCOL-1)    /* Maximum columns in line buffer - 1 */

#define MAXTTYCONTROL   8      /* Maximum TTY control character list */
#define TTYINTERRUPT    0
#define TTYERASE        1
#define TTYKILL         2
#define TTYEOF          3
#define TTYSUSPEND      4
#define TTYREPRINT      5
#define TTYDISCARD      6
#define TTYWERASE       7

/* input modes */
#define LTERM0_RAW_MODE         0
#define LTERM1_CANONICAL_MODE   1
#define LTERM2_EDIT_MODE        2
#define LTERM3_COMPLETION_MODE  3

/* completion request codes */
#define LTERM_NO_COMPLETION       0
#define LTERM_TAB_COMPLETION      1
#define LTERM_HISTORY_COMPLETION  2

/* output modes */
#define LTERM0_STREAM_MODE  0
#define LTERM1_SCREEN_MODE  1
#define LTERM2_LINE_MODE    2

/* character/line action codes */
#define LTERM_INSERT_ACTION 0
#define LTERM_DELETE_ACTION 1
#define LTERM_ERASE_ACTION  2

/* List of characters escaped in XML */
#define LTERM_AMP_ESCAPE    0
#define LTERM_LT_ESCAPE     1
#define LTERM_GT_ESCAPE     2
#define LTERM_QUOT_ESCAPE   3
#define LTERM_APOS_ESCAPE   4

#define LTERM_XML_ESCAPES   5
#define LTERM_PLAIN_ESCAPES 3

/* Maximum no. of characters in an XML escape sequence (excluding NUL) */
#define LTERM_MAXCHAR_ESCAPE 6

/* input buffer pipe header "character" count */
#define PIPEHEADER  2

/* input buffer pipe header components */
#define PHDR_CHARS  0
#define PHDR_TYPE   1

/* LTERM read in/out parameter structure */
struct LtermRead {
  UNICHAR *buf;     /* Pointer to Unicode character buffer   (IN param) */
  UNISTYLE *style;  /* Pointer to character style buffer     (IN param) */
  int max_count;    /* max. number of characters in buffers  (IN param) */
  int read_count;   /* actual number of characters in buffers */
  int opcodes;      /* Returned opcodes */
  int opvals;       /* Returned opvalues */
  int buf_row;      /* row at which to display buffer data */
  int buf_col;      /* starting column at which to display buffer data */
  int cursor_row;   /* final cursor row position */
  int cursor_col;   /* final cursor column position */
};

/* LTERM input structure: managed by lterm_write */
struct LtermInput {

  UNICHAR inputBuf[PIPEHEADER+MAXCOL]; /* holds data read from input buffer
                                        * pipe
                                        */
  int inputBufBytes;           /* Count of bytes already read in */

  int inputMode;               /* input mode:
                                * 0 = raw mode
                                * 1 = canonical mode
                                * 2 = edit + canonical mode
                                * 3 = completion + edit + canonical mode
                                */

  int escapeFlag;              /* processing ESCAPE in line mode */
  int escapeCSIFlag;           /* processing ESCAPE Code Sequence Introducer */
  int escapeCSIArg;            /* ESCAPE Code Sequence Argument value */

  int inputOpcodes;            /* input opcodes */

  int clearInputLine;          /* true if input line buffer needs to be
                                * cleared (after echoing) */

  UNICHAR inputLine[MAXCOL];   /* input line buffer:
                                  only MAXCOL-1 characters should actually
                                  be inserted in the buffer, to allow
                                  one character padding if necessary */

  int inputChars;              /* length of input line (characters);
                                * should never exceed MAXCOL-1,
                                * to allow for null termination
                                */
  int inputCols;               /* number of displayed columns in input line;
                                * a column corresponds to a single
                                * plain text character transmitted to the
                                * subordinate process, although it may occupy
                                * multiple character positions, e.g.,
                                * e.g., &lt represents "<"
                                */
  int inputGlyphs;             /* number of displayed glyphs in input line;
                                * a glyph corresponds to a logical column
                                * on the layout, i.e., a single Unicode
                                * character or an XML element, such as an
                                * iconic representiion of an URI.
                                */

  unsigned short inputColCharIndex[MAXCOL]; /* starting character index of
                                             * each column, including the
                                             * cursor column at end of line.
                                             * (inputCols+1 values should be
                                             * defined)
                                             */

  unsigned short inputGlyphCharIndex[MAXCOL]; /* starting character index
                                               * of each glyph, including
                                               * empty glyph at the end of
                                               * the line.
                                               * (inputGlyphs+1 values
                                               * should be defined)
                                               */
  unsigned short inputGlyphColIndex[MAXCOL]; /* starting column index of
                                              * each glyph
                                              * (inputGlyphs+1 values)
                                              */

  int inputCursorGlyph;           /* current input cursor glyph number (>=0) */
};

/* LtermOutput poll structure index count and values */
#define POLL_COUNT 3
#define POLL_INPUTBUF 0
#define POLL_STDOUT   1
#define POLL_STDERR   2

/* LTERM output structure: managed by lterm_read */
struct LtermOutput {
  struct POLLFD pollFD[POLL_COUNT];  /* output polling structure */
  long callbackTag[POLL_COUNT]; /* GTK callback tag for each FD (0 if none) */
  int nfds;                     /* count of "files" to be polled */
  int outputMode;               /* output mode:
                                 * 0 = full screen mode
                                 * 1 = line mode
                                 * 2 = command line mode
                                 */

  UNICHAR streamTerminator[MAXSTREAMTERM]; /* stream terminator buffer */
  int streamOpcodes;            /* Stream opcodes */
  int savedOutputMode;          /* saved output mode (prior to stream mode) */

  int insertMode;               /* character insert mode */
  int automaticNewline;         /* automatic newline mode */

  UNISTYLE styleMask;           /* current output style mask */

  char rawOUTBuf[MAXRAWINCOMPLETE]; /* incomplete raw STDOUT buffer */
  int rawOUTBytes;               /* incomplete raw STDOUT byte count */

  char rawERRBuf[MAXRAWINCOMPLETE]; /* incomplete raw STDERR buffer */
  int rawERRBytes;               /* incomplete raw STDERR byte count */

  UNICHAR decodedOutput[MAXCOL]; /* decoded output buffer:
                                    only MAXCOL-1 characters should actually
                                    be inserted in the buffer, to allow
                                    one character padding if necessary */
  UNISTYLE decodedStyle[MAXCOL]; /* decoded output style buffer */
  int decodedChars;              /* decoded character count;
                                    should never exceed MAXCOL-1 */
  int incompleteEscapeSequence;  /* Incomplete ESCAPE sequence flag */

  UNICHAR outputLine[MAXCOL];  /* output line buffer (processed);
                                  only MAXCOL-1 characters should actually
                                  be inserted in the buffer, to allow
                                  one character padding if necessary */
  UNISTYLE outputStyle[MAXCOL]; /* output style buffer for each character */

  int outputChars;             /* length of output line (characters)
                                  should never exceed MAXCOL-1 */
  int outputCursorChar;        /* output cursor character position (>=0) */
  int promptChars;             /* prompt character count */
  int outputModifiedChar;      /* leftmost modified character in output line */

  int cursorRow, cursorCol;    /* screen cursor row and column */
  int returnedCursorRow, returnedCursorCol;
                               /* returned screen cursor row and column */

  int topScrollRow, botScrollRow;    /* top and bottom scrolling rows */

  int modifiedCol[MAXROW];     /* first modified column in each row;
                                  -1 if no column has been modified */

  UNICHAR  *screenChar;        /* Screen character array (nRows*nCols long) */
  UNISTYLE *screenStyle;       /* Screen style array (nRows*nCols long) */
};

/* LTERM process structure: managed by lterm_create, lterm_close */
struct LtermProcess {
  PROCESS processID;           /* process ID */
  FILEDESC processIN;          /* process input pipe */
  FILEDESC processOUT;         /* process output pipe */
  FILEDESC processERR;         /* process error pipe */
};

/* line terminal (LTERM) structure: managed by lterm_open, lterm_close */
struct lterms { 
  int opened;                      /* LTERM opened status flag */
  int suspended;                   /* LTERM suspended flag:
                                    * an LTERM is suspended when an error
                                    * occurs, to prevent further I/O operations
                                    * which have unpredictable results.
                                    * The LTERM still needs to be closed to
                                    * release any resources used by it.
                                    * (a suspended LTERM is still open)
                                    */

  MUTEX_DECLARE(adminMutex);       /* LTERM administrative mutex */
  MUTEX_DECLARE(outputMutex);      /* LTERM output thread mutex */

  int adminMutexLocked;            /* administrative mutex lock status */
  int outputMutexLocked;           /* output mutex lock status */

  FILEDESC writeBUFFER, readBUFFER; /* input character BUFFER pipe */

  int options;                     /* TTY options */
  int ptyMode;                     /* pseudo-TTY mode flag */
  int noTTYEcho;                   /* no TTY echo flag */
  int disabledInputEcho;           /* disabled input echo flag */
  int restoreInputEcho;            /* restore input echo flag */

  int processType;                 /* Process type code */
  int maxInputMode;                /* maximum allowed input mode value */
  int readERRfirst;                /* Read STDERR before STDOUT */
  int interleave;                  /* interleave STDERR/STDOUT flag */
  UNICHAR control[MAXTTYCONTROL];  /* TTY control characters */


  int commandNumber;               /* output command number
                                    * (0 if not command line)
                                    */
  unsigned short lastCommandNum;   /* last command number */
  int completionRequest;           /* command completion request code:
                                    * LTERM_NO_COMPLETION, or
                                    * LTERM_TAB_COMPLETION, or
                                    * LTERM_HISTORY_COMPLETION
                                    */
  int completionChars;             /* command completion insert count */

  int inputBufRecord;              /* True if input buffer contains record */

  int inputLineBreak;              /* True if input line was transmitted
                                    * and plain text copy saved in echo buffer
                                    */
  UNICHAR echoLine[MAXCOL];        /* Plain text of echo line */
  int echoChars;                   /* Count of echo characters */

  int nRows;                       /* Number of rows */
  int nCols;                       /* Number of columns */
  int xPixels;                     /* Number of X pixels in screen */
  int yPixels;                     /* Number of Y pixels in screen */

  UNICHAR promptRegexp[MAXPROMPT]; /* prompt regular expression
                                      JUST A LIST OF DELIMITERS AT PRESENT */

  char cookie[MAXCOOKIESTR];       /* cookie string */
  char shellInitStr[MAXSHELLINITCMD][MAXSHELLINITSTR];
                                   /* shell initialization strings */
  int shellInitCommands;           /* shell init command count */

  struct ptys pty;                 /* pseudo-tty (PTY) stream info for LTERM */
  struct LtermProcess ltermProcess; /* LTERM process structure */
  struct LtermInput ltermInput;     /* LTERM input structure */
  struct LtermOutput ltermOutput;   /* LTERM output structure */
};

/* LTERM global variables */
typedef struct { 
  int initialized;                  /* Initialization flag */
  struct lterms *termList[MAXTERM]; /* List of LTERMS */
  MUTEX_DECLARE(listMutex);         /* Thread lock to access to LTERM list */
  UNICHAR metaDelimiter;            /* Meta command delimiter (usually :) */
  char escapeChars[LTERM_XML_ESCAPES+1]; /* String of chars escaped in XML */
  UNICHAR escapeSeq[LTERM_XML_ESCAPES][LTERM_MAXCHAR_ESCAPE+1];
                                        /* XML character escape sequences */
  int escapeLen[LTERM_XML_ESCAPES];     /* XML char. escape sequence lengths */
} LtermGlobal;

extern LtermGlobal ltermGlobal;


/* Visible prototypes */

/* ltermIO.c */
int ltermInterruptOutput(struct lterms *lts);
int ltermSendLine(struct lterms *lts, UNICHAR uch,
                  int echoControl, int completionCode);
int ltermRead(struct lterms *lts, struct LtermRead *ltr, int timeout);

/* ltermInput.c */
int ltermPlainTextInput(struct lterms *lts,
                        const UNICHAR *buf, int count, int *opcodes);
int ltermCancelCompletion(struct lterms *lts);
int ltermInsertChar(struct LtermInput *lti, UNICHAR uch);
void ltermSwitchToRawMode(struct lterms *lts);
void ltermClearInputLine(struct lterms *lts);
int ltermDeleteGlyphs(struct LtermInput *lti, int count);
int ltermSendData(struct lterms *lts, const UNICHAR *buf, int count);
int ltermSendChar(struct lterms *lts, const char *buf, int count);

/* ltermOutput.c */
int ltermProcessOutput(struct lterms *lts, int *opcodes, int *opvals,
                       int *oprow);
int ltermReceiveData(struct lterms *lts, int readERR);
void ltermClearOutputLine(struct lterms *lts);
int ltermClearOutputScreen(struct lterms *lts);
int ltermSwitchToStreamMode(struct lterms *lts, int streamOpcodes,
                            const UNICHAR *streamTerminator);
int ltermSwitchToScreenMode(struct lterms *lts);
int ltermSwitchToLineMode(struct lterms *lts);

/* ltermEscape.c */
int ltermProcessEscape(struct lterms *lts, const UNICHAR *buf,
                       int count, const UNISTYLE *style, int *consumed,
                       int *opcodes, int *opvals, int *oprow);
int ltermInsDelEraseChar(struct lterms *lts, int count, int action);
int ltermInsDelEraseLine(struct lterms *lts, int count, int row, int action);

#endif  /* _LTERMPRIVATE_H */
