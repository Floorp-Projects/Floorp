/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef TxLog_h__
#define TxLog_h__

#ifdef TX_EXE

#ifdef PR_LOGGING
#include <stdio.h>

#define PRLogModuleInfo void
#define PR_NewLogModule(_name)
#define PR_FREE_IF(_name)

typedef enum PRLogModuleLevel {
    PR_LOG_NONE = 0,                /* nothing */
    PR_LOG_ALWAYS = 1,              /* always printed */
    PR_LOG_ERROR = 2,               /* error messages */
    PR_LOG_WARNING = 3,             /* warning messages */
    PR_LOG_DEBUG = 4,               /* debug messages */

    PR_LOG_NOTICE = PR_LOG_DEBUG,   /* notice messages */
    PR_LOG_WARN = PR_LOG_WARNING,   /* warning messages */
    PR_LOG_MIN = PR_LOG_DEBUG,      /* minimal debugging messages */
    PR_LOG_MAX = PR_LOG_DEBUG       /* maximal debugging messages */
} PRLogModuleLevel;


#define PR_LOG(_name, _level, _message)  printf##_message

#else

#define PR_LOG(_name, _level, _message) 

#endif

#else
#include "prlog.h"
#include "prmem.h"
#endif

#ifdef PR_LOGGING
class txLog
{
public:
    static PRLogModuleInfo *xpath;
    static PRLogModuleInfo *xslt;
};

#define TX_LG_IMPL \
    PRLogModuleInfo * txLog::xpath = 0; \
    PRLogModuleInfo * txLog::xslt = 0

#define TX_LG_CREATE \
    txLog::xpath = PR_NewLogModule("xpath"); \
    txLog::xslt  = PR_NewLogModule("xslt")

#define TX_LG_DELETE \
    PR_FREEIF(txLog::xpath); \
    PR_FREEIF(txLog::xslt)

#else

#define TX_LG_IMPL
#define TX_LG_CREATE
#define TX_LG_DELETE

#endif

#endif
