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

/* tracelog.c: Tracing/logging module implementation
 */

/* public declarations */
#include <stdlib.h>
#include <stdarg.h>

#include "unistring.h"
#include "tracelog.h"

#ifdef USE_NSPR_BASE
#include "prlog.h"
#endif

/* private declarations */

/* TRACELOG global variable structure */
TlogGlobal tlogGlobal;

static int initGlobal = 0;

/** Initializes all TRACELOG operations and sets filestream for trace/log
 * output. Setting filestream to NULL suppresses all output.
 * (documented in tracelog.h)
 */
void tlog_init(FILE* fileStream)
{
  int imodule;

#ifdef DEBUG_LTERM
  fprintf(stderr, "tlog_init:\n");
#endif

  /* Do not re-initialize */
  if (initGlobal)
    return;

  initGlobal = 1;

  /* Error output stream */
  tlogGlobal.errorStream = fileStream;

  /* Debugging is disabled initially */
  tlogGlobal.debugOn = 0;

  for (imodule=0; imodule<TLOG_MAXMODULES; imodule++) {
    tlogGlobal.messageLevel[imodule] = 0;
    tlogGlobal.functionList[imodule] = NULL;
  }

  return;
}


/** Sets TRACELOG display levels for module IMODULE
 * (documented in tracelog.h)
 * @return 0 on success, or -1 on error.
 */
int tlog_set_level(int imodule, int messageLevel, const char *functionList)
{
  int j;

#ifdef DEBUG_LTERM
  fprintf(stderr, "tlog_set_level:%d, %d\n", imodule, messageLevel);
#endif

  if ((imodule < 0) || (imodule >= TLOG_MAXMODULES))
    return -1;

  /* Message level */
  tlogGlobal.messageLevel[imodule] = messageLevel;

  /* Free function list string */
  free(tlogGlobal.functionList[imodule]);

  if (functionList == NULL) {
    tlogGlobal.functionList[imodule] = NULL;

  } else {
    /* Duplicate function list string */
    int slen = strlen(functionList);
    char *stem;

    if (slen > 1000) slen = 1000;

    stem = malloc((unsigned int) slen+3);
    strncpy(&stem[1], functionList, (unsigned int) slen);
    stem[0] = ':';
    stem[slen+1] = ':';
    stem[slen+2] = '\0';

    tlogGlobal.functionList[imodule] = stem;

    if (messageLevel > 0) {
      tlog_warning("tlog_set_level: module %d, functionList=\"%s\"\n",
                                    imodule, tlogGlobal.functionList[imodule]);
    }
  }

  /* Turn on debugging only if needed */
  tlogGlobal.debugOn = 0;

  if (tlogGlobal.errorStream != NULL) {
    for (j=0; j<TLOG_MAXMODULES; j++) {
      if ((tlogGlobal.messageLevel[j] > 0) ||
          (tlogGlobal.functionList != NULL))
        tlogGlobal.debugOn = 1;
    }
  }

  if (messageLevel > 0) {
    tlog_warning("tlog_set_level: module %d, messageLevel=%d\n",
                                    imodule, messageLevel);
  }

  return 0;
}


/** Determines whether trace/log message is to be displayed for specified
 * module at specified message level.
 * @return 1 (true) if message should be displayed, 0 otherwise
 */
int tlog_test(int imodule, char *procstr, int level)
{
  if (tlogGlobal.errorStream == NULL)
    return 0;

  if ((imodule < 0) || (imodule >= TLOG_MAXMODULES))
    return 0;

  if ( (level <= tlogGlobal.messageLevel[imodule]) ||
         ((tlogGlobal.functionList[imodule] != NULL) &&
          ( (strstr(tlogGlobal.functionList[imodule],procstr) != NULL) ||
             (strstr(procstr,tlogGlobal.functionList[imodule]) != NULL)) ) ) {
    /* Display message */
#if defined(USE_NSPR_BASE) && !defined(DEBUG_LTERM)
    PR_LogPrint("%s%2d: ", procstr, level);
#else
    fprintf(tlogGlobal.errorStream, "%s%2d: ", procstr, level);
#endif
    return 1;
  }

  return 0;
}


/** Displays an error message on the TRACELOG filestream */
void tlog_message(const char *format, ...)
{
  va_list ap;             /* pointer to variable length argument list */

  if (tlogGlobal.errorStream == NULL)
    return;

  va_start(ap, format);   /* make ap point to first unnamed arg */
  vfprintf(tlogGlobal.errorStream, format, ap);
  va_end(ap);             /* clean up */
  return;
}


/** Displays a warning message on the TRACELOG filestream */
void tlog_warning(const char *format, ...)
{
  va_list ap;             /* pointer to variable length argument list */

  if ((tlogGlobal.errorStream == NULL) || !tlogGlobal.debugOn)
    return;

  va_start(ap, format);   /* make ap point to first unnamed arg */
  vfprintf(tlogGlobal.errorStream, format, ap);
  va_end(ap);             /* clean up */
  return;
}


#ifdef _UNISTRING_H
#define MAXCOL 1024            /* Maximum columns in line buffer */

/** Displays an Unicode message on the TRACELOG filestream */
void tlog_unichar(const UNICHAR *buf, int count)
{
  if (tlogGlobal.errorStream == NULL)
    return;

#if defined(USE_NSPR_BASE) && !defined(DEBUG_LTERM)
  PR_LogPrint("U(%d):\n", count);
#else
  fprintf(tlogGlobal.errorStream, "U(%d): ", count);
  ucsprint(tlogGlobal.errorStream, buf, count);
  fprintf(tlogGlobal.errorStream, "\n");
#endif

}
#endif
