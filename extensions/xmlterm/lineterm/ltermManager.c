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

/* ltermManager.c: LTERM control operations
 */

/* public declarations */
#include "lineterm.h"

/* private declarations */
#include "ltermPrivate.h"

/* LTERM global variables */
LtermGlobal ltermGlobal;

#define CHECK_IF_VALID_LTERM_NUMBER(procname,lterm)  \
  if ((lterm < 0) || (lterm >= MAXTERM)) {           \
    LTERM_ERROR("procname: Error - LTERM index %d out of range\n", \
                lterm);                              \
    return -1;                                       \
  }

/* Macros for use in thread synchronization */
#define GLOBAL_LOCK   MUTEX_LOCK(ltermGlobal.listMutex)
#define GLOBAL_UNLOCK MUTEX_UNLOCK(ltermGlobal.listMutex)

/* The following macros assume that ltermGlobal.listMutex has already been
 * locked.
 * A UNILOCK is lock that can be locked only by one thread at a time;
 * attempts to lock it by another thread result in an error return.
 * This can be used to detect violations of thread discipline.
 */

#define SWITCH_GLOBAL_TO_UNILOCK(procname,mutex,mutex_flag)  \
  if (lts->mutex_flag) {                 \
    LTERM_ERROR("procname: Error - MUTEX mutex already locked\n", \
                lts->mutex);   \
    MUTEX_UNLOCK(ltermGlobal.listMutex); \
    return -1;                           \
  }                                      \
  MUTEX_LOCK(lts->mutex);                \
  lts->mutex_flag = 1;                   \
  MUTEX_UNLOCK(ltermGlobal.listMutex);

#define RELEASE_UNILOCK(mutex,mutex_flag)  \
  lts->mutex_flag = 0;      \
  MUTEX_UNLOCK(lts->mutex);


int ltermClose(struct lterms *lts);
static int ltermShellInit(struct lterms *lts, int all);
static int ltermCreatePipe(FILEDESC *writePIPE, FILEDESC *readPIPE);
static int ltermCreateProcess(struct LtermProcess *ltp,
                              char *const argv[], int nostderr, int noexport);
static void ltermDestroyProcess(struct LtermProcess *ltp);


/** Initializes all LTERM operations
 * (documented in the LTERM interface)
 * @return 0 on success, or -1 on error.
 */
int lterm_init(int messageLevel)
{
  if (!ltermGlobal.initialized) {
    int result, j;
    /* Assert that INTs have at least 32-bit precision.
     * This assumption underlies the choice of various bit codes.
     */
    assert(sizeof(int) >= 4);

    if (sizeof(UNICHAR) == 2) {
      /* Assert that the maximum column count fits into an UNICHAR.
       * This assumption is used for input buffer pipe I/O by lterm_write
       * and ltermWrite, but could be relaxed by using different buffer record
       * structure.
       */
      assert(MAXCOL <= 0x7FFF);
    }

    /* Initialize lock for accessing LTERMs array */
    if (MUTEX_INITIALIZE(ltermGlobal.listMutex) != 0)
      return -1;

    /* Initialize trace/log module for STDERR;
     * this could perhaps be eliminated, if TLOG_PRINT is redefined to PR_LOG
     * and so on, with the USE_NSPR_IO option enabled.
     */
    tlog_init(stderr);
    result = tlog_set_level(LTERM_TLOG_MODULE, messageLevel, NULL);

    /* Meta input delimiter */
    ltermGlobal.metaDelimiter = U_COLON;

    /* String of five ASCII characters escaped in XML;
     * only the first four characters are escaped in HTML;
     * it is sufficient to escape just the first three characters, "&<>"
     * when inserting plain text (outside markup) in XML/HTML fragments
     */
    ltermGlobal.escapeChars[LTERM_AMP_ESCAPE]  = '&';
    ltermGlobal.escapeChars[LTERM_LT_ESCAPE]   = '<';
    ltermGlobal.escapeChars[LTERM_GT_ESCAPE]   = '>';
    ltermGlobal.escapeChars[LTERM_QUOT_ESCAPE] = '\"';
    ltermGlobal.escapeChars[LTERM_APOS_ESCAPE] = '\'';
    ltermGlobal.escapeChars[LTERM_XML_ESCAPES] = 0;

    /* List of XML character escape sequences (Unicode) */
    ucscopy(ltermGlobal.escapeSeq[LTERM_AMP_ESCAPE],  "&amp;",
              LTERM_MAXCHAR_ESCAPE+1);
    ucscopy(ltermGlobal.escapeSeq[LTERM_LT_ESCAPE],   "&lt;",
              LTERM_MAXCHAR_ESCAPE+1);
    ucscopy(ltermGlobal.escapeSeq[LTERM_GT_ESCAPE],   "&gt;",
              LTERM_MAXCHAR_ESCAPE+1);
    ucscopy(ltermGlobal.escapeSeq[LTERM_QUOT_ESCAPE], "&quot;",
              LTERM_MAXCHAR_ESCAPE+1);
    ucscopy(ltermGlobal.escapeSeq[LTERM_APOS_ESCAPE], "&apos;",
              LTERM_MAXCHAR_ESCAPE+1);

    /* Escape sequence lengths (including delimiters) */
    for (j=0; j<LTERM_XML_ESCAPES; j++) {
      ltermGlobal.escapeLen[j] = ucslen(ltermGlobal.escapeSeq[j]);
      assert(ltermGlobal.escapeLen[j] <= LTERM_MAXCHAR_ESCAPE);
    }

    /* LTERM global initialization flag */
    ltermGlobal.initialized = 1;

  } else {
    LTERM_WARNING("lterm_init: Warning - already initialized\n");
  }

  return 0;
}


/** Creates a new LTERM object and returns its descriptor index but
 *  does not open it for I/O
 * (documented in the LTERM interface)
 * @return lterm descriptor index (>= 0) on success, or
 *         -1 on error
 */
int lterm_new()
{
  int lterm;
  struct lterms *lts;

  if (!ltermGlobal.initialized) {
    LTERM_ERROR("lterm_new: Error - call lterm_init() to initialize LTERM library\n");
    return -1;
  }

  LTERM_LOG(lterm_new,10,("Creating LTERM ...\n"));

  /* Allocate memory for new LTERMS structure: needs clean-up */
  lts = (struct lterms *) MALLOC(sizeof(struct lterms));

  if (lts == NULL) {
    LTERM_ERROR("lterm_new: Error - failed to allocate memory for LTERM\n");
    return -1;
  }

  GLOBAL_LOCK;

  for (lterm=0; lterm < MAXTERM; lterm++) {
    if (ltermGlobal.termList[lterm] == NULL)
      break;
  }

  if (lterm == MAXTERM) {
    LTERM_ERROR( "lterm_new: Error - too many LTERMS; recompile with increased MAXTERM\n");
    FREE(lts);  /* Clean up memory allocation for LTERMS structure */
    GLOBAL_UNLOCK;
    return -1;
  }
  
  /* Insert LTERM pointer in list of LTERMs */
  ltermGlobal.termList[lterm] = lts;

  MUTEX_INITIALIZE(lts->adminMutex);

  /* Administrative thread inactive */
  lts->adminMutexLocked = 0;

  /* LTERM not yet opened for I/O */
  lts->opened = 0;

  GLOBAL_UNLOCK;

  LTERM_LOG(lterm_new,11,("created lterm = %d\n", lterm));

  return lterm;
}


/* Macro to close and delete LTERM */
#define LTERM_OPEN_ERROR_RETURN(lterm,lts,errmsg) \
  LTERM_ERROR(errmsg);  \
  ltermClose(lts);      \
  RELEASE_UNILOCK(adminMutex,adminMutexLocked) \
  lterm_delete(lterm);  \
  return -1;

/** Opens an LTERM for I/O
 * (documented in the LTERM interface)
 * @return 0 on success, or -1 on error
 */
int lterm_open(int lterm, char *const argv[],
               const char* cookie, const char* init_command,
               const UNICHAR* prompt_regexp, int options, int process_type,
               int rows, int cols, int x_pixels, int y_pixels,
               lterm_callback_func_t callback_func, void *callback_data)
{
  int nostderr, noPTY, j;
  struct ptys ptyStruc;
  struct lterms *lts;
  struct LtermInput *lti;
  struct LtermOutput *lto;
  struct LtermProcess *ltp;
  char *const *cargv;
  char *defaultArgs[3];

  if (!ltermGlobal.initialized) {
    LTERM_ERROR("lterm_open: Error - call lterm_init() to initialize LTERM library\n");
    return -1;
  }

  CHECK_IF_VALID_LTERM_NUMBER(lterm_open,lterm)

  LTERM_LOG(lterm_open,10,("Opening LTERM %d\n", lterm));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL) {
    LTERM_ERROR("lterm_open: Error - LTERM %d not created using lterm_new\n",
                lterm);
    GLOBAL_UNLOCK;
    return -1;
  }

  if (lts->opened) {
    LTERM_ERROR("lterm_open: Error - LTERM %d is already open\n",
                lterm);
    GLOBAL_UNLOCK;
    return -1;
  }

  SWITCH_GLOBAL_TO_UNILOCK(lterm_open,adminMutex,adminMutexLocked)

  /* Pointers to LTERM process/input/output structures */
  ltp = &(lts->ltermProcess);
  lti = &(lts->ltermInput);
  lto = &(lts->ltermOutput);

  /* Initialize flags/pointers/file descriptors that need to be cleaned up */
  lts->suspended = 0;
  lts->ptyMode   = 0;

  /* Initialize TTY control characters */
  assert(TTYWERASE < MAXTTYCONTROL);
  lts->control[TTYINTERRUPT] = U_CTL_C;
  lts->control[TTYERASE]     = U_DEL;
  lts->control[TTYKILL]      = U_CTL_U;
  lts->control[TTYEOF]       = U_CTL_D;
  lts->control[TTYSUSPEND]   = U_CTL_Z;
  lts->control[TTYREPRINT]   = U_CTL_R;
  lts->control[TTYDISCARD]   = U_CTL_O;
  lts->control[TTYWERASE]    = U_CTL_W;

  lts->writeBUFFER =  NULL_FILEDESC;
  lts->readBUFFER =   NULL_FILEDESC;

  /* Clear input line break flag */
  lts->inputLineBreak = 0;

  /* Clear input buffer */
  lti->inputBufBytes = 0;
  lts->inputBufRecord = 0;

  /* Clear styleMask */
  lto->styleMask = 0;

  /* Clear incomplete raw output buffers */
  lto->rawOUTBytes = 0;
  lto->rawERRBytes = 0;

  /* Clear decoded output buffer */
  lto->decodedChars = 0;
  lto->incompleteEscapeSequence = 0;

  /* Set initial screen size */
  lts->nRows = rows;
  lts->nCols = cols;
  lts->xPixels = y_pixels;
  lts->yPixels = x_pixels;

  /* Clear screen buffer */
  lto->screenChar  = NULL;
  lto->screenStyle = NULL;

  lts->outputMutexLocked = 0;

  MUTEX_INITIALIZE(lts->outputMutex);

  /* AT THIS POINT, LTERMCLOSE MAY BE CALLED FOR CLEAN-UP */

  /* Create input buffer pipe; needs clean-up */
  if (ltermCreatePipe(&(lts->writeBUFFER), &(lts->readBUFFER)) != 0) {
    LTERM_OPEN_ERROR_RETURN(lterm,lts,
                "lterm_open: Error - input buffer pipe creation failed\n")
  }

  LTERM_LOG(lterm_open,11,("Created input buffer pipe\n", lterm));

  defaultArgs[0] = NULL;
  defaultArgs[1] = "-i";
  defaultArgs[2] = NULL;

  if (argv != NULL) {
    /* Use supplied command arguments */
    cargv= argv;

  } else {
    /* Default command arguments */
    defaultArgs[0] = (char *) getenv("SHELL");
    if (defaultArgs[0] == NULL)
      defaultArgs[0] = "/bin/sh";
    cargv= defaultArgs;
  }

  if (process_type >= 0) {
    /* Specified process type code */
    lts->processType = process_type;

  } else {
    /* Determine process type code from path name */
    char *lastSlash;
    char *tailStr;

    /* Find the last occurrence of slash */
    lastSlash = (char *) strrchr(cargv[0], '/');
    if (lastSlash == NULL) {
      tailStr = cargv[0];
    } else {
      tailStr = lastSlash + 1;
    }

    if (strcmp(tailStr,"sh") == 0) {
      lts->processType = LTERM_SH_PROCESS;

    } else if (strcmp(tailStr,"ksh") == 0) {
      lts->processType = LTERM_KSH_PROCESS;

    } else if (strcmp(tailStr,"bash") == 0) {
      lts->processType = LTERM_BASH_PROCESS;

    } else if (strcmp(tailStr,"csh") == 0) {
      lts->processType = LTERM_CSH_PROCESS;

    } else if (strcmp(tailStr,"tcsh") == 0) {
      lts->processType = LTERM_TCSH_PROCESS;

    } else {
      lts->processType = LTERM_UNKNOWN_PROCESS;
    }
  }

  LTERM_LOG(lterm_open,11,("process type code = %d\n", lts->processType));

  /* TTY options */
  lts->options = options;
  lts->noTTYEcho = (options & LTERM_NOECHO_FLAG) != 0;
  lts->disabledInputEcho = 1;
  lts->restoreInputEcho = 1;

  nostderr = (options & LTERM_STDERR_FLAG) == 0;

#if defined(NO_PTY) || defined(USE_NSPR_IO)
  /* Current implementation of pseudo-TTY is incompatible with the NSPR I/O
     option; set the USE_NSPR_IO option only on platforms where
     PTYSTREAM is not implemented.
     Reason for incompatibility: cannot combine the UNIX-style integral
     PTY file descriptor and NSPR's PRFileDesc in the same poll function
     call.
  */
  noPTY = 1;
#else
  noPTY = (options & LTERM_NOPTY_FLAG);
#endif

  if (noPTY) {
    /* Create pipe process: needs clean-up */
    if (ltermCreateProcess(ltp, cargv, nostderr, 0) == -1) {

      LTERM_OPEN_ERROR_RETURN(lterm,lts,
            "lterm_open: Pipe process creation failed\n")
    }

  } else {
    /* Create pseudo-TTY with blocking I/O: needs clean-up */
    int noblock = 1;
    int noexport = 0;
    int debugPTY = (tlogGlobal.messageLevel[LTERM_TLOG_MODULE] > 1);
    int errfd = nostderr?-1:-2;

    LTERM_LOG(lterm_open,11,
              ("errfd=%d, noecho=%d, noblock=%d, noexport=%d, debugPTY=%d\n",
               errfd, lts->noTTYEcho, noblock, noexport, debugPTY));
#ifndef NO_PTY
    if (pty_create(&ptyStruc, cargv,
                   lts->nRows, lts->nCols, lts->xPixels, lts->yPixels,
                   errfd, noblock, lts->noTTYEcho, noexport, debugPTY) == -1) {
      LTERM_OPEN_ERROR_RETURN(lterm,lts,
            "lterm_open: Error - PTY creation failed\n")
    }
#endif  /* !NO_PTY */

    /* Copy PTY structure */
    lts->pty = ptyStruc;
    lts->ptyMode = 1;
  }

  /* Set up LtermOutput polling structure */
  lto->pollFD[POLL_INPUTBUF].fd = lts->readBUFFER;
  lto->pollFD[POLL_INPUTBUF].POLL_EVENTS = POLL_READ;

  if (lts->ptyMode) {
#ifndef USE_NSPR_IO
    lto->pollFD[POLL_STDOUT].fd = lts->pty.ptyFD;
    lto->pollFD[POLL_STDOUT].POLL_EVENTS = POLL_READ;

    if (VALID_FILEDESC(lts->pty.errpipeFD)) {
      lto->pollFD[POLL_STDERR].fd = lts->pty.errpipeFD;
      lto->pollFD[POLL_STDERR].POLL_EVENTS = POLL_READ;
      lto->nfds = POLL_COUNT;
    } else {
        lto->nfds = POLL_COUNT-1;
    }
#else
    /* Current implementation of pseudo-TTY incompatible with USE_NSPR_IO */
    LTERM_OPEN_ERROR_RETURN(lterm,lts,
           "lterm_open: Error - pseudo-TTY incompatible with USE_NSPR_IO\n")
#endif

  } else {
    lto->pollFD[POLL_STDOUT].fd = ltp->processOUT;
    lto->pollFD[POLL_STDOUT].POLL_EVENTS = POLL_READ;

    if (VALID_FILEDESC(ltp->processERR)) {
      lto->pollFD[POLL_STDERR].fd = ltp->processERR;
      lto->pollFD[POLL_STDERR].POLL_EVENTS = POLL_READ;
      lto->nfds = POLL_COUNT;
    } else {
      lto->nfds = POLL_COUNT-1;
    }
  }

  /* No callbacks initially associated with this LTERM */
  for (j=0; j<lto->nfds; j++)
    lto->callbackTag[j] = 0;

  /* Determine whether STDERR should be read before STDOUT */
  if (lts->processType == LTERM_TCSH_PROCESS) {
    lts->readERRfirst = 0;
  } else {
    lts->readERRfirst = 1;
  }

  /* Determine whether STDERR and STDOUT should be interleaved */
  lts->interleave = 1;

  /* Determine maximum value for input mode */
  if (options & LTERM_NOCANONICAL_FLAG)
    lts->maxInputMode = LTERM0_RAW_MODE;

  else if (options & LTERM_NOEDIT_FLAG)
    lts->maxInputMode = LTERM1_CANONICAL_MODE;

  else if ((options & LTERM_NOCOMPLETION_FLAG) ||
           lts->noTTYEcho)
    lts->maxInputMode = LTERM2_EDIT_MODE;

  else {
    if ((lts->processType == LTERM_BASH_PROCESS) ||
        (lts->processType == LTERM_TCSH_PROCESS))
      lts->maxInputMode = LTERM3_COMPLETION_MODE;
    else
      lts->maxInputMode = LTERM2_EDIT_MODE;
  }

  LTERM_LOG(lterm_open,11,("options=0x%x, processType=%d, readERRfirst=%d, maxInputMode=%d\n",
                lts->options, lts->processType, lts->readERRfirst, lts->maxInputMode));
  LTERM_LOGUNICODE(lterm_open,11,(prompt_regexp, (int) ucslen(prompt_regexp) ));

  if (cookie != NULL) {
    /* Copy cookie string (no more than MAXCOOKIESTR-1 characters) */
    if (strlen(cookie) > MAXCOOKIESTR-1) {
      LTERM_WARNING("lterm_open: Warning - cookie string truncated\n");
    }
    strncpy(lts->cookie, cookie, MAXCOOKIESTR-1);
    lts->cookie[MAXCOOKIESTR-1] = '\0';
  } else {
    /* No cookie */
    lts->cookie[0] = '\0';
  }

  LTERM_LOG(lterm_open,11,("cookie='%s'\n",lts->cookie));

  /* Shell initialization commands have not been sent yet */
  lts->shellInitCommands = 0;

  /* NOTE: Shell init commands are stored in reverse order of execution */

  if (init_command != NULL) {
    /* User supplied shell init command */

    if (strlen(init_command) <= MAXSHELLINITSTR-1) {
      int cmd = lts->shellInitCommands++;
      assert(cmd < MAXSHELLINITCMD);

      (void) strncpy(lts->shellInitStr[cmd], init_command, MAXSHELLINITSTR-1);
      lts->shellInitStr[cmd][MAXSHELLINITSTR-1] = '\0';

    } else {
      LTERM_WARNING("lterm_open: Warning - user init command too long\n");
    }
  }

  if (lts->processType != LTERM_UNKNOWN_PROCESS) {
    /* Process dependent shell init command */
    int result;
    char* shellInitFormat;

    if ((lts->processType == LTERM_CSH_PROCESS) ||
      (lts->processType == LTERM_TCSH_PROCESS)) {
      /* C-shell family */
      shellInitFormat = "setenv LTERM_COOKIE '%s'\n";

    } else {
      /* Bourne-shell family */
      shellInitFormat = "LTERM_COOKIE='%s'; export LTERM_COOKIE\n";
    }

    /* **** WATCH OUT FOR BUFFER OVERFLOW!!! *** */
    if (strlen(shellInitFormat)-4+strlen(lts->cookie) <= MAXSHELLINITSTR-1) {

      int cmd = lts->shellInitCommands++;
      assert(cmd < MAXSHELLINITCMD);

      sprintf(lts->shellInitStr[cmd], shellInitFormat, lts->cookie);
      lts->shellInitStr[cmd][MAXSHELLINITSTR-1] = '\0';

    } else {
      LTERM_WARNING("lterm_open: Warning - process init command too long\n");
    }
  }

  for (j=lts->shellInitCommands-1; j>0; j--) {
    LTERM_LOG(lterm_open,11,("shellInitStr%d='%s'\n",j,lts->shellInitStr[j]));
  }

  /* Initialize regular expression string matching command prompt */
  if ((ucslen(prompt_regexp) == 0) ||
      (ucslen(prompt_regexp) > MAXPROMPT-1)) {
    LTERM_OPEN_ERROR_RETURN(lterm,lts,
         "lterm_open: Error - prompt_regexp string too short/long\n")
  }

  ucsncpy( lts->promptRegexp, prompt_regexp, MAXPROMPT);

  /* Command number counter */
  lts->lastCommandNum = 0;

  /* Command completion request code */
  lts->completionRequest = LTERM_NO_COMPLETION;

  /* Clear output line buffer and switch to line output mode */
  ltermClearOutputLine(lts);
  lto->outputMode = LTERM2_LINE_MODE;
  lto->styleMask = 0;

  /* Reset terminal modes */
  lto->insertMode = 0;
  lto->automaticNewline = 0;

  /* Clear input line buffer */
  ltermClearInputLine(lts);

  /* Clear input line flag not yet set */
  lti->clearInputLine = 0;

  /* Reset input opcodes */
  lti->inputOpcodes = 0;

  /* Open LTERM */
  lts->opened = 1;

  /* Initialize any callbacks associated with this LTERM */
  if (callback_func != NULL) {
#if !defined(USE_NSPR_IO) && defined(USE_GTK_WIDGETS)
    LTERM_LOG(lterm_open,11,("Initializing %d callbacks for LTERM %d\n",
                               lto->nfds, lterm));

    for (j=0; j<lto->nfds; j++) {
      lto->callbackTag[j] = gdk_input_add(lto->pollFD[j].fd,
                                          GDK_INPUT_READ,
                                          callback_func,
                                          (gpointer) callback_data);
      if (lto->callbackTag[j] == 0) {
        LTERM_OPEN_ERROR_RETURN(lterm,lts,
                    "lterm_open: Error - unable to add callback\n")
      }
    }
#else
    LTERM_OPEN_ERROR_RETURN(lterm,lts,
      "lterm_open: Error - callbacks not implemented for this configuration\n")
#endif
  }


  RELEASE_UNILOCK(adminMutex,adminMutexLocked)

  LTERM_LOG(lterm_open,11,
      ("outputMode=%d, inputMode=%d, maxInputMode=%d\n",
      lto->outputMode, lti->inputMode,lts->maxInputMode ));

  return 0;
}


/** Closes an LTERM described by structure pointed to by LTS.
 * (Auxiliary routine for lterm_close, lterm_delete, and lterm_close_all)
 * NOTE: This routine should not attempt to acquire the global lock,
 *       since it is called from LTERM_CLOSE_ALL under a global lock.
 * @return 0 on success, or -1 on error.
 */
int ltermClose(struct lterms *lts)
{
  struct LtermInput *lti;
  struct LtermOutput *lto;
  UNICHAR closeMessage[2] = {0, LTERM_WRITE_CLOSE_MESSAGE};
  int j;

  LTERM_LOG(ltermClose,10,("Closing LTERM\n"));

  assert(lts != NULL);

  /* Admin thread must be active */
  assert(lts->adminMutexLocked);

  /* Suspend LTERM to prevent any further I/O operations */
  lts->suspended = 1;

  /* Pointers to LTERM input/output structures */
  lti = &(lts->ltermInput);
  lto = &(lts->ltermOutput);

  /* Send close signal to any terminate blocked read operation */
  WRITE(lts->writeBUFFER, closeMessage, (SIZE_T)(2*sizeof(UNICHAR)) );

  /* Wait for read operations to complete */
  MUTEX_LOCK(lts->outputMutex);
  MUTEX_UNLOCK(lts->outputMutex);

  /* Destroy output mutex */
  MUTEX_DESTROY(lts->outputMutex);

  /* Close input buffer pipe */
  if (VALID_FILEDESC(lts->writeBUFFER))
    CLOSE(lts->writeBUFFER);

  if (VALID_FILEDESC(lts->readBUFFER))
    CLOSE(lts->readBUFFER);

#ifdef USE_GTK_WIDGETS
  /* Remove any callbacks associated with this LTERM */
  for (j=0; j<lto->nfds; j++) {
    if (lto->callbackTag[j] != 0) {
      gdk_input_remove((gint) lto->callbackTag[j]);
      lto->callbackTag[j] = 0;
    }
  }
#endif

  if (lts->ptyMode) {
    /* Close PTY */
#ifndef NO_PTY
    pty_close(&(lts->pty));
#endif  /* !NO_PTY */

  } else {
    /* Destroy process */
    ltermDestroyProcess(&(lts->ltermProcess));
  }

  /* Free full screen buffers */
  if (lto->screenChar != NULL)
    FREE(lto->screenChar);

  if (lto->screenStyle != NULL)
    FREE(lto->screenStyle);

  /* Close LTERM */
  lts->opened = 0;

  LTERM_LOG(ltermClose,11,("LTERM closed\n"));

  return 0;
}


/** Closes an LTERM
 * (documented in the LTERM interface)
 * @return 0 on success, or -1 on error.
 */
int lterm_close(int lterm)
{
  struct lterms *lts;
  int returnCode;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_close,lterm)

  LTERM_LOG(lterm_close,10,("Closing LTERM %d\n", lterm));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL) {
    /* Forgiving return for non-existent LTERM */
    GLOBAL_UNLOCK;
    return 0;
  }

  if (!lts->opened) {
    LTERM_WARNING("lterm_close: Error - LTERM %d not opened\n", lterm);
    GLOBAL_UNLOCK;
    return -1;
  }

  SWITCH_GLOBAL_TO_UNILOCK(lterm_close,adminMutex,adminMutexLocked)

  returnCode = ltermClose(lts);

  RELEASE_UNILOCK(adminMutex,adminMutexLocked)

  return returnCode;
}


/** Deletes an LTERM object, closing it if necessary
 * (documented in the LTERM interface)
 * @return 0 on success, or -1 on error.
 */
int lterm_delete(int lterm)
{
  struct lterms *lts;
  int returnCode;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_delete,lterm)

  LTERM_LOG(lterm_delete,10,("Closing LTERM %d\n", lterm));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL) {
    /* Forgiving return for non-existent LTERM */
    GLOBAL_UNLOCK;
    return 0;
  }

  /* Remove LTERM structure from list */
  ltermGlobal.termList[lterm] = NULL;

  SWITCH_GLOBAL_TO_UNILOCK(lterm_open,adminMutex,adminMutexLocked)

  returnCode = 0;
  if (lts->opened) {
    /* Opened LTERM; close it */
    returnCode = ltermClose(lts);
  }

  RELEASE_UNILOCK(adminMutex,adminMutexLocked)

  MUTEX_DESTROY(lts->adminMutex);

  /* Free memory for LTERMS structure */
  FREE(lts);

  LTERM_LOG(lterm_delete,11,("LTERM deleted\n"));

  return returnCode;
}


/* closes all LTERMs (documented in the LTERM interface) */
void lterm_close_all()
{
  int lterm;
  struct lterms *lts;

  LTERM_LOG(lterm_close_all,10,("\n"));

  GLOBAL_LOCK;

  for (lterm=0; lterm < MAXTERM; lterm++) {
    lts = ltermGlobal.termList[lterm];

    if ((lts != NULL) && lts->opened) {
      /* Opened LTERM; close it, disregarding return code */
      lts->adminMutexLocked = 1;
      MUTEX_LOCK(lts->adminMutex);

      /* NOTE,10, ("lterm_close_all:\n"******** This call is being made from within a global lock;
       *                POTENTIAL FOR DEADLOCK?
       */
      ltermClose(lts);

      lts->adminMutexLocked = 0;
      MUTEX_UNLOCK(lts->adminMutex);
    }
  }

  GLOBAL_UNLOCK;
}


/** Sets input echo flag for LTERM (documented in the LTERM interface) */
int lterm_setecho(int lterm, int echo_flag)
{
  struct lterms *lts;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_setecho,lterm)

  LTERM_LOG(lterm_setecho,10,("LTERM=%d, echo_flag=%d\n", lterm, echo_flag));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL || !lts->opened || lts->suspended) {
    /* LTERM is deleted/closed/suspended */
    if (lts == NULL)
      LTERM_WARNING("lterm_setecho: Warning - LTERM %d not active\n", lterm);
    GLOBAL_UNLOCK;
    return -2;
  }

  if (lts->shellInitCommands > 0) {
    /* send shell initialization string */
    if (ltermShellInit(lts,1) != 0) {
      GLOBAL_UNLOCK;
      return -1;
    }
  }

  lts->disabledInputEcho = !echo_flag;
  lts->restoreInputEcho = 0;

  GLOBAL_UNLOCK;

  return 0;
}


/** Resizes the line terminal indexed by LTERM to new row/column count.
 * Called from the output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_resize(int lterm, int rows, int cols)
{
  struct lterms *lts;
  struct LtermOutput *lto;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_resize,lterm)

  LTERM_LOG(lterm_resize,10,("Resizing LTERM=%d, rows=%d, cols=%d\n",
                             lterm, rows, cols));

  if ((rows <= 0)  || (cols <= 0))
    return -1;

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL || !lts->opened || lts->suspended) {
    /* LTERM is deleted/closed/suspended */
    if (lts == NULL)
      LTERM_WARNING("lterm_resize: Warning - LTERM %d not active\n", lterm);
    GLOBAL_UNLOCK;
    return -2;
  }

  if ((rows == lts->nRows) && (cols == lts->nCols)) {
    /* Nothing to do */
    GLOBAL_UNLOCK;
    return 0;
  }

  lto = &(lts->ltermOutput);

  LTERM_LOG(lterm_resize,12,("lto->outputMode=%d\n",
                             lto->outputMode));

  /* Free full screen buffers */
  if (lto->screenChar != NULL)
    FREE(lto->screenChar);

  if (lto->screenStyle != NULL)
    FREE(lto->screenStyle);

  lto->screenChar  = NULL;
  lto->screenStyle = NULL;

  /* Resize screen */
  lts->nRows = rows;
  lts->nCols = cols;

  if (lto->outputMode == LTERM1_SCREEN_MODE) {
    /* Clear screen */
    if (ltermClearOutputScreen(lts) != 0)
      return -1;
  }

  if (lts->ptyMode) {
    /* Resize PTY */
#ifndef NO_PTY
    if (pty_resize(&(lts->pty), lts->nRows, lts->nCols, 0, 0) != 0) {
      GLOBAL_UNLOCK;
      return -1;
    }
#endif  /* !NO_PTY */
  }

  GLOBAL_UNLOCK;

  return 0;
}


/** Sets cursor position in line terminal indexed by LTERM.
 * NOT YET IMPLEMENTED
 * Row numbers increase upward, starting from 0.
 * Column numbers increase rightward, starting from 0.
 * Called from the output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_setcursor(int lterm, int row, int col)
{
  struct lterms *lts;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_setcursor,lterm)

  LTERM_LOG(lterm_setcursor,10,
            ("Setting cursor, LTERM=%d row=%d, col=%d (NOT YET IMPLEMENTED)\n",
             lterm, row, col));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL || !lts->opened || lts->suspended) {
    /* LTERM is deleted/closed/suspended */
    if (lts == NULL)
      LTERM_WARNING("lterm_setcursor: Warning - LTERM %d not active\n", lterm);
    GLOBAL_UNLOCK;
    return -2;
  }

  GLOBAL_UNLOCK;

  return 0;
}


/** Writes a string to LTERM (documented in the LTERM interface) */
int lterm_write(int lterm, const UNICHAR *buf, int count, int dataType)
{
  struct lterms *lts;
  FILEDESC writeBUFFER;
  int byteCount, sentCount, packetCount, j;
  UNICHAR temLine[PIPEHEADER+MAXCOL], uch;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_write,lterm)

  LTERM_LOG(lterm_write,10,("Writing to LTERM %d\n", lterm));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL || !lts->opened || lts->suspended) {
    /* LTERM is deleted/closed/suspended */
    if (lts == NULL)
      LTERM_WARNING("lterm_write: Warning - LTERM %d not active\n", lterm);
    GLOBAL_UNLOCK;
    return -2;
  }

  assert(VALID_FILEDESC(lts->writeBUFFER));

  /* Save copy of input buffer file descriptor */
  writeBUFFER = lts->writeBUFFER;

  if (lts->restoreInputEcho == 1) {
    /* Restore TTY echo on input */
    lts->restoreInputEcho = 0;
    lts->disabledInputEcho = 0;
  }

  if (lts->shellInitCommands > 0) {
    /* send shell initialization string */
    if (ltermShellInit(lts,1) != 0) {
      GLOBAL_UNLOCK;
      return -1;
    }
  }

  GLOBAL_UNLOCK;

  assert(count >= 0);

  sentCount = 0;
  while (sentCount < count) {
    packetCount = count - sentCount;  /* always > 0 */

    if (packetCount > MAXCOLM1)
      packetCount = MAXCOLM1;

    if ( (dataType == LTERM_WRITE_PLAIN_INPUT) ||
         (dataType == LTERM_WRITE_PLAIN_OUTPUT) ) {
      /* Plain text data; break at control characters,
       * but break at ESCape character only if not the first character;
       * this ensures that escape sequences fit into one record
       */

      for (j=0; j<packetCount; j++) {
        uch = buf[j+sentCount];
        temLine[j+PIPEHEADER] = uch;

        if ((uch < (UNICHAR)U_SPACE) &&
            ((uch != (UNICHAR)U_ESCAPE) || (j > 0)))
          break;
      }

      if (j<packetCount)
        packetCount = j+1;

      if ( (packetCount > 1) &&
           (temLine[PIPEHEADER+packetCount-1] == U_ESCAPE) ) {
        /* Save any terminal ESCape character for next packet, unless alone */
        packetCount--;
      }

    } else {
      /* XML element data (must represent complete element) */
      for (j=0; j<packetCount; j++)
        temLine[j+PIPEHEADER] = buf[j+sentCount];
    }

    byteCount = (PIPEHEADER+packetCount)*sizeof(UNICHAR);

    /* Header contains packet character count and data type */
    temLine[0]         = (UNICHAR) packetCount;
    temLine[PHDR_TYPE] = (UNICHAR) dataType;

    LTERM_LOGUNICODE(lterm_write,12,(&temLine[PIPEHEADER], packetCount));

    if (WRITE(lts->writeBUFFER,temLine,(SIZE_T)byteCount) != byteCount) {
      LTERM_ERROR("lterm_write: Error in writing to input pipe buffer\n");
      return -1;
    }

    LTERM_LOG(lterm_write,11,
              ("wrote %d characters of type %d data to input buffer pipe\n",
               packetCount, dataType));

    sentCount += packetCount;
  }

  return 0;
}


/** Reads a (possibly incomplete) line from LTERM
 * (documented in the LTERM interface)
 */
int lterm_read(int lterm, int timeout, UNICHAR *buf, int count,
               UNISTYLE *style, int *opcodes, int *opvals,
               int *buf_row, int *buf_col, int *cursor_row, int *cursor_col)
{
  struct lterms *lts;
  int returnCode, temCode;
  struct LtermRead ltrStruc;

  CHECK_IF_VALID_LTERM_NUMBER(lterm_read,lterm)

  LTERM_LOG(lterm_read,10,("Reading from LTERM %d\n", lterm));

  GLOBAL_LOCK;

  lts = ltermGlobal.termList[lterm];

  if (lts == NULL || !lts->opened || lts->suspended) {
    /* LTERM is deleted/closed/suspended */
    if (lts == NULL)
      LTERM_WARNING("lterm_read: Warning - LTERM %d not active\n", lterm);

    *opcodes = 0;
    *opvals = 0;
    *buf_row = 0;
    *buf_col = 0;
    *cursor_row = 0;
    *cursor_col = 0;

    GLOBAL_UNLOCK;
    return -2;
  }

  SWITCH_GLOBAL_TO_UNILOCK(lterm_read,outputMutex,outputMutexLocked)

  /* Copy "input" parameters to LtermRead structure */
  ltrStruc.buf = buf;
  ltrStruc.style = style;
  ltrStruc.max_count = count;

  /* Call auxiliary function to read data */
  temCode = ltermRead(lts, &ltrStruc, timeout);

  if (temCode == 0) {
    /* Return count of characters read */
    returnCode = ltrStruc.read_count;
  } else {
    /* Error return */
    returnCode = temCode;
  }

  /* Copy "output" parameters from LtermRead structure */
  *opcodes = ltrStruc.opcodes;
  *opvals = ltrStruc.opvals;
  *buf_row = ltrStruc.buf_row;
  *buf_col = ltrStruc.buf_col;
  *cursor_row = ltrStruc.cursor_row;
  *cursor_col = ltrStruc.cursor_col;

  if (returnCode == -1) {
    /* Error return code; suspend TTY */
    LTERM_WARNING("lterm_read: Warning - LTERM %d suspended due to error\n", lterm);
    lts->suspended = 1;
  }

  RELEASE_UNILOCK(outputMutex,outputMutexLocked)

  GLOBAL_LOCK;
  if ((*opcodes != 0) && (lts->shellInitCommands > 0)) {
    /* send shell initialization string */
    if (ltermShellInit(lts,0) != 0) {
      GLOBAL_UNLOCK;
      return -1;
    }
  }
  GLOBAL_UNLOCK;

  LTERM_LOG(lterm_read,11,("return code = %d, opcodes=0x%x, opvals=%d\n",
                           returnCode, *opcodes, *opvals));

  return returnCode;
}


/** Sends shell initialization commands, if any
 * (WORKS UNDER GLOBAL_LOCK)
 * @param all if true, send all init commands, else send just one command
 * @return 0 on success and -1 on error.
 */
static int ltermShellInit(struct lterms *lts, int all)
{
  int shellInitLen, lowCommand, j;

  if (lts->shellInitCommands <= 0)
    return 0;

  LTERM_LOG(ltermShellInit,20,("sending shell initialization string\n"));

  if (all) {
    /* Send all commands */
    lowCommand = 0;
  } else {
    /* Send single command */
    lowCommand = lts->shellInitCommands - 1;
  }

  for (j=lts->shellInitCommands-1; j>=lowCommand; j--) {
    /* Send shell init command and decrement count */
    lts->shellInitCommands--;

    shellInitLen = strlen(lts->shellInitStr[j]);

    if (shellInitLen > 0) {
      /* Send shell initialization string */
      UNICHAR temLine[PIPEHEADER+MAXCOL];
      int remaining, encoded, byteCount;

      utf8toucs(lts->shellInitStr[j], shellInitLen,
                temLine+PIPEHEADER, MAXCOL,
                1, &remaining, &encoded);
      if (remaining > 0) {
        LTERM_ERROR(
         "ltermShellInit: Shell init command %d string too long\n", j+1);
        return -1;
      }

      temLine[0]         = (UNICHAR) encoded;
      temLine[PHDR_TYPE] = (UNICHAR) LTERM_WRITE_PLAIN_INPUT;
      byteCount = (PIPEHEADER+encoded)*sizeof(UNICHAR);

      if (WRITE(lts->writeBUFFER, temLine, (SIZE_T)byteCount) != byteCount) {
        LTERM_ERROR("ltermShellInit: Error in writing to input pipe buffer\n");
        return -1;
      }
    }
  }

  return 0;
}


/** Creates unidirectional pipe which can be written to and read from using
 * the file descriptors writePIPE and readPIPE.
 * @return 0 on success and -1 on error.
 */
static int ltermCreatePipe(FILEDESC *writePIPE, FILEDESC *readPIPE)
{
  FILEDESC pipeFD[2];

  LTERM_LOG(ltermCreatePipe,20,("Creating pipe\n"));

  /* Assume that pipe is unidirectional, although in some UNIX
     systems pipes are actually bidirectional */

  if (PIPE(pipeFD) == -1) {
    LTERM_ERROR("ltermCreatePipe: Error - pipe creation failed\n");
    return -1;
  }

  /* Copy pipe file descriptor */
  *readPIPE  = pipeFD[0];
  *writePIPE = pipeFD[1];

  return 0;
}


/** Creates a new process to execute the command line contained in array
 * ARGV and redirects its standard I/O and standard error through pipes.
 * The process details are stored in the structure pointed to by LTP.
 * If NOSTDERR is true, STDERR is redirected to STDOUT.
 * If NOEXPORT is true, then the current environment is not exported
 * to the new process.
 * @return 0 on success and -1 on error.
 */
static int ltermCreateProcess(struct LtermProcess *ltp,
                              char *const argv[], int nostderr, int noexport)
{
  FILEDESC stdIN, stdOUT, stdERR;
#ifdef USE_NSPR_IO
  PRProcessAttr *processAttr;
#else
  long child_pid;
#endif

  LTERM_LOG(ltermCreateProcess,20,("Creating process %s, nostderr=%d, noexport=%d\n", argv[0], nostderr, noexport));

  if (nostderr) {
    /* No STDERR pipe */
    ltp->processERR = NULL_FILEDESC;
    stdERR=           NULL_FILEDESC;
  } else {
    /* Create STDERR pipe: needs clean-up */
    FILEDESC pipeFD[2];

    if (PIPE(pipeFD) == -1) {
      LTERM_ERROR("ltermCreateProcess: Error - STDERR pipe creation failed\n");
      return -1;
    }

    /* Copy pipe file descriptor */
    ltp->processERR = pipeFD[0];
    stdERR = pipeFD[1];
  }

  /* Create I/O pipes: needs clean up */
  if ( (ltermCreatePipe(&(ltp->processIN), &stdIN) != 0) ||
       (ltermCreatePipe(&stdOUT, &(ltp->processOUT)) != 0) )
    return -1;

#ifdef USE_NSPR_IO
  /* Create new process attribute structure */
  processAttr = PR_NewProcessAttr();

  /* Redirect standard error for process */
  if (VALID_FILEDESC(stdERR))
    PR_ProcessAttrSetStdioRedirect(processAttr, (PRSpecialFD) 2, stdERR);
  else
    PR_ProcessAttrSetStdioRedirect(processAttr, (PRSpecialFD) 2, stdOUT);

  /* Redirect standard I/O for process */
  PR_ProcessAttrSetStdioRedirect(processAttr, (PRSpecialFD) 0, stdIN);
  PR_ProcessAttrSetStdioRedirect(processAttr, (PRSpecialFD) 1, stdOUT);

  /* Create NSPR process; do not export environment */
  ltp->processID = PR_CreateProcess(argv[0], argv,
                                    (char *const *)0, processAttr);

  if (!VALID_PROCESS(ltp->processID)) {
    LTERM_ERROR("ltermCreateProcess: Error in creating NSPR process %s\n",
                 argv[0]);
    return -1;
  }

  LTERM_LOG(ltermCreateProcess,21,("Created NSPR process\n"));

#else  /* not USE_NSPR_IO */

  /* Fork a child process (FORK) */
  child_pid = fork();
  if (child_pid < 0) {
    LTERM_ERROR("ltermCreateProcess: Error - fork failed\n");
    return -1;
  }

  LTERM_LOG(ltermCreateProcess,21,("vfork child pid = %d\n", child_pid));

  if (child_pid == 0) {
    /* Child process */
    int fdMax, fd;

    /* Redirect to specified descriptor or to PTY */
    if (VALID_FILEDESC(stdERR)) {
      /* Redirect STDERR to specified file descriptor */
      if (dup2(stdERR, 2) == -1) {
        LTERM_ERROR("ltermCreateProcess: Error - failed dup2 for specified stderr\n");
        return -1;
      }

    } else {
      /* Redirect STDERR to STDOUT pipe */
      if (dup2(stdOUT, 2) == -1) {
        LTERM_ERROR("ltermCreateProcess: Error - failed dup2 for default stderr\n");
        return -1;
      }
    }

    /* Redirect STDIN and STDOUT to pipes */
    if (dup2(stdIN, 0) == -1) {
      LTERM_ERROR("ltermCreateProcess: Error - failed dup2 for stdin\n");
      return -1;
    }

    if (dup2(stdOUT, 1) == -1) {
      LTERM_ERROR("ltermCreateProcess: Error - failed dup2 for stdout\n");
      return -1;
    }

    /* Close all other file descriptors in child process */
    fdMax = sysconf(_SC_OPEN_MAX);
    for (fd = 3; fd < fdMax; fd++)
      close(fd);

    if (argv != NULL) {
      /* Execute specified command with arguments */
      if (noexport)
        execve(argv[0], argv, NULL);
      else
        execvp(argv[0], argv);

      LTERM_ERROR("ltermCreateProcess: Error in executing command %s\n",
                  argv[0]);
      return -1;
    }
  }

  /* Parent process; copy child pid */
  ltp->processID = child_pid;

  /* Close child-side pipe end(s) */
  if (stdIN != stdOUT)
    close(stdIN);

  close(stdOUT);

  /* Close writable end of STDERR pipe in parent process */
  if (VALID_FILEDESC(stdERR))
    close(stdERR);

#endif  /* not USE_NSPR_IO */

  LTERM_LOG(ltermCreateProcess,21,("return code = 0\n"));

  return 0;
}

/** Destroys process pointed by LTP
 */
static void ltermDestroyProcess(struct LtermProcess *ltp)
{

  LTERM_LOG(ltermDestroyProcess,20,("Destroying process\n"));

  if (VALID_PROCESS(ltp->processID)) {
    /* Kill process */
#ifdef USE_NSPR_IO
    /* **NOTE** Possible memory leak here, because processID is not freed? */
    PR_KillProcess(ltp->processID);
#else  /* not USE_NSPR_IO */
    LTERM_LOG(ltermDestroyProcess,21,("Killing process %d\n",ltp->processID));
    kill(ltp->processID, SIGKILL);
#endif /* not USE_NSPR_IO */

    ltp->processID = NULL_PROCESS;
  }

  /* Close process pipes (assumed to be unidirectional) */
  if (VALID_FILEDESC(ltp->processERR))
    CLOSE(ltp->processERR);

  if (VALID_FILEDESC(ltp->processOUT))
    CLOSE(ltp->processOUT);

  if (VALID_FILEDESC(ltp->processIN))
    CLOSE(ltp->processIN);
}
