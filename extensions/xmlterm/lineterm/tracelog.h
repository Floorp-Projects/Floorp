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

/* tracelog.h: Tracing/logging module header
 * CPP options:
 *   DEBUG_LTERM:          to enable debugging output
 *   _UNISTRING_H:   for unicode messages compatible with "unistring.h"
 */

#ifndef _TRACELOG_H

#define _TRACELOG_H 1

#include <stdio.h>
#include <string.h>

/* Trace/log macros (to be used after call to tlog_init to initialize):
 * TLOG_ERROR:   error message macro, e.g.,
 *                                          TLOG_ERROR(format, val1, val2);
 * TOG_WARNING:  warning message macro, e.g.,
 *                                          TLOG_WARNING(format, val1, val2);
 * TLOG_PRINT:   message logging macro, e.g., (no terminating semicolon)
 *                                         TLOG_PRINT(10,(format, val1, val2));
 * (if UNISTRING module is being used)
 * TLOG_UNICHAR: Unicode string logging macro, e.g., (no terminating semicolon)
 *                                          TLOG_UNICHAR(10,(label,str,count));
 */

/* Max. number of modules recognized by TRACELOG */
# define TLOG_MAXMODULES 50

#ifdef  __cplusplus
extern "C" {
#endif

/* Initializes all TRACELOG operations;
 * needs to be called before any other trace/log calls.
 *
 * FILESTREAM is the file stream to be used to print messages.
 *
 * Normally, only error messages are sent to FILESTREAM.
 * If FILESTREAM is null, all output, including error output, is suppressed.
 */

void tlog_init(FILE* fileStream);

/** Set diagnostic message display level for module no. IMODULE.
 * (0 <= IMODULE < TLOG_MAXMODULES)
 *
 * MESSAGELEVEL (>=0) specifies the diagnostic message display level:
 *  only diagnostic messages with level values >= MESSAGELEVEL are printed
 *  (For example, level 10, 11, ...: outermost level;
 *                level 20, 21, ...: next inner level;
 *                ...
 *                level 50, 51, ...: innermost level)
 *
 * The message SUBLEVEL threshold is defined as MESSAGELEVEL%10
 * (ranging from 0 to 9).
 * Only those diagnostic messages with sublevel values >= SUBLEVEL threshold
 * are displayed
 * Usually, the SUBLEVEL threshold values are interpreted as
 *  0 =>     print single message per selected procedure.
 *  1...9 => print only messages upto selected sublevel.
 *
 * Setting MESSAGELEVEL to zero and FUNCTIONLIST to null for all modules
 * disables debugging and printing of warning messages.
 * (This is the initial configuration following the call to lterm_init.)
 * Setting MESSAGELEVEL to 1 for atleast one module enables debugging and
 * causes warning messages for all modules to be printed.
 *
 * FUNCTIONLIST is a colon-separated string of function names, e.g.,
 * "func_a:func_b".
 * Trace/log messages for functions in this list are always output
 * if debugging is enabled provided the sublevel values exceed the threshold,
 * regardless of full message level values.
 * If FUNCTIONLIST contains a single method name without a class name, or a
 * class name without a method name, then the missing portion is assumed
 * to be wild-carded.
 *
 * Returns 0 on success, -1 otherwise (i.e., for invalid module numbers)
 */

int tlog_set_level(int imodule, int messageLevel, const char *functionList);

int tlog_test(int imodule, char *procname, int level);
void tlog_message(const char *format, ...);
void tlog_warning(const char *format, ...);

/* TRACELOG global variables */
typedef struct { 
  FILE *errorStream;                     /* file stream for logging */
  int debugOn;
  int messageLevel[TLOG_MAXMODULES];
  char *functionList[TLOG_MAXMODULES];   /* list of functions to be debugged */
} TlogGlobal;

extern TlogGlobal tlogGlobal;

#ifdef  __cplusplus
}
#endif

#if defined(USE_NSPR_BASE) && !defined(DEBUG_LTERM)
#include "prlog.h"
#define TLOG_MESSAGE PR_LogPrint
#else
#define TLOG_MESSAGE tlog_message
#endif

#define TLOG_ERROR TLOG_MESSAGE

#define TLOG_WARNING if (tlogGlobal.debugOn) TLOG_MESSAGE

#define TLOG_PRINT(imodule,procname,level,args) \
do {                                            \
  if (tlogGlobal.debugOn && tlog_test(imodule,":" #procname ":",level)) {  \
      TLOG_MESSAGE args;                                                  \
  }                                             \
} while(0)

#ifdef _UNISTRING_H
void tlog_unichar(const UNICHAR *buf, int count);

#define TLOG_UNICHAR(imodule,procname,level,args) \
do {                                              \
if (tlogGlobal.debugOn && tlog_test(imodule,":" #procname ":",level)) {  \
    tlog_unichar args;                            \
  }                                               \
} while(0)
#endif

#endif  /* _TRACELOG_H */
