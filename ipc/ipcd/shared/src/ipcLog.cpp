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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "ipcLog.h"

#ifdef IPC_LOGGING

#include <string.h>
#include <ctype.h>

#include "prenv.h"
#include "prprf.h"
#include "prthread.h"
#include "plstr.h"

PRBool ipcLogEnabled = PR_FALSE;
char ipcLogPrefix[10] = {0};

//-----------------------------------------------------------------------------
// UNIX
//-----------------------------------------------------------------------------
#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#include <sys/types.h>
#include <unistd.h>

static inline PRUint32
WritePrefix(char *buf, PRUint32 bufLen)
{
    return PR_snprintf(buf, bufLen, "[%u:%p] %s ",
                       (unsigned) getpid(),
                       PR_GetCurrentThread(),
                       ipcLogPrefix);
}
#endif

//-----------------------------------------------------------------------------
// WIN32
//-----------------------------------------------------------------------------
#ifdef XP_WIN
#include <windows.h>

static inline PRUint32
WritePrefix(char *buf, PRUint32 bufLen)
{
    return PR_snprintf(buf, bufLen, "[%u:%p] %s ",
                       GetCurrentProcessId(),
                       PR_GetCurrentThread(),
                       ipcLogPrefix);
}
#endif

//-----------------------------------------------------------------------------
// logging API impl
//-----------------------------------------------------------------------------

void
IPC_InitLog(const char *prefix)
{
    if (PR_GetEnv("IPC_LOG_ENABLE")) {
        ipcLogEnabled = PR_TRUE;
        PL_strncpyz(ipcLogPrefix, prefix, sizeof(ipcLogPrefix));
    }
}

void
IPC_Log(const char *fmt, ... )
{
    va_list ap;
    va_start(ap, fmt);
    PRUint32 nb = 0;
    char buf[512];

    if (ipcLogPrefix[0])
        nb = WritePrefix(buf, sizeof(buf));

    PR_vsnprintf(buf + nb, sizeof(buf) - nb, fmt, ap);
    buf[sizeof(buf) - 1] = '\0';

    fwrite(buf, strlen(buf), 1, stdout);

    va_end(ap);
}

void
IPC_LogBinary(const PRUint8 *data, PRUint32 len)
{
    PRUint32 i, j, ln;
    for (i=0; i<len; ) {
        char line[100] = "";
        const PRUint8 *p;

        ln = 0;
        
        p = &data[i];
        for (j=0; j<PR_MIN(8, len - i); ++j, ++p)
            ln += PR_snprintf(line + ln, sizeof(line) - ln, "%02x  ", *p);

        for (; ln < 32; ++ln)
            line[ln] = ' ';

        p = &data[i];
        for (j=0; j<PR_MIN(8, len - i); ++j, ++p)
            ln += PR_snprintf(line + ln, sizeof(line) - ln, "%c", isprint(*p) ? *p : '.');

        line[ln] = '\0';

        i += (p - &data[i]);

        LOG(("%s\n", line));
    }
}

#endif
