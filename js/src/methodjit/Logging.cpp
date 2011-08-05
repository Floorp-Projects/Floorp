/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   Julian Seward <jseward@acm.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "jsutil.h"
#include "MethodJIT.h"
#include "Logging.h"

#if defined(JS_METHODJIT_SPEW)

static bool LoggingChecked = false;
static uint32 LoggingBits = 0;

static const char *ChannelNames[] =
{
#define _(name) #name,
    JSPEW_CHAN_MAP(_)
#undef  _
};

void
js::JMCheckLogging()
{
    /* Not MT safe; races on Logging{Checked,Bits}. */
    if (LoggingChecked)
        return;
    LoggingChecked = true;
    const char *env = getenv("JMFLAGS");
    if (!env)
        return;
    if (strstr(env, "help")) {
        fflush(NULL);
        printf(
            "\n"
            "usage: JMFLAGS=option,option,option,... where options can be:\n"
            "\n"
            "  help          show this message\n"
            "  abort/aborts  ???\n"
            "  scripts       ???\n"
            "  profile       ???\n"
#ifdef DEBUG
            "  pcprofile     Runtime hit counts of every JS opcode executed\n"
            "  jsops         JS opcodes\n"
#endif
            "  insns         JS opcodes and generated insns\n"
            "  vmframe       VMFrame contents\n"
            "  pics          PIC patching activity\n"
            "  slowcalls     Calls to slow path functions\n"
            "  analysis      LICM and other analysis behavior\n"
            "  regalloc      Register allocation behavior\n"
            "  inlin         Call inlining behavior\n"
            "  recompile     Dynamic recompilations\n"
            "  full          everything not affecting codegen\n"
            "\n"
        );
        exit(0);
        /*NOTREACHED*/
    }
    if (strstr(env, "abort") || strstr(env, "aborts"))
        LoggingBits |= (1 << uint32(JSpew_Abort));
    if (strstr(env, "scripts"))
        LoggingBits |= (1 << uint32(JSpew_Scripts));
    if (strstr(env, "profile"))
        LoggingBits |= (1 << uint32(JSpew_Prof));
#ifdef DEBUG
    if (strstr(env, "jsops"))
        LoggingBits |= (1 << uint32(JSpew_JSOps));
#endif
    if (strstr(env, "insns"))
        LoggingBits |= (1 << uint32(JSpew_Insns) | (1 << uint32(JSpew_JSOps)));
    if (strstr(env, "vmframe"))
        LoggingBits |= (1 << uint32(JSpew_VMFrame));
    if (strstr(env, "pics"))
        LoggingBits |= (1 << uint32(JSpew_PICs));
    if (strstr(env, "slowcalls"))
        LoggingBits |= (1 << uint32(JSpew_SlowCalls));
    if (strstr(env, "analysis"))
        LoggingBits |= (1 << uint32(JSpew_Analysis));
    if (strstr(env, "regalloc"))
        LoggingBits |= (1 << uint32(JSpew_Regalloc));
    if (strstr(env, "recompile"))
        LoggingBits |= (1 << uint32(JSpew_Recompile));
    if (strstr(env, "inlin"))
        LoggingBits |= (1 << uint32(JSpew_Inlining));
    if (strstr(env, "full"))
        LoggingBits |= 0xFFFFFFFF;
}

js::ConditionalLog::ConditionalLog(bool logging)
    : oldBits(LoggingBits), logging(logging)
{
    if (logging)
        LoggingBits = 0xFFFFFFFF;
}

js::ConditionalLog::~ConditionalLog() {
    if (logging)
        LoggingBits = oldBits;
}

bool
js::IsJaegerSpewChannelActive(JaegerSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    return !!(LoggingBits & (1 << uint32(channel)));
}

void
js::JaegerSpew(JaegerSpewChannel channel, const char *fmt, ...)
{
    JS_ASSERT(LoggingChecked);

    if (!(LoggingBits & (1 << uint32(channel))))
        return;

    fprintf(stderr, "[jaeger] %-7s  ", ChannelNames[channel]);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    /* fprintf(stdout, "\n"); */
}

#endif

