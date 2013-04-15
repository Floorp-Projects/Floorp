/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "jsutil.h"
#include "MethodJIT.h"
#include "Logging.h"

#include "jsobjinlines.h"

#if defined(JS_METHODJIT_SPEW)

static bool LoggingChecked = false;
static uint32_t LoggingBits = 0;

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
        LoggingBits |= (1 << uint32_t(JSpew_Abort));
    if (strstr(env, "scripts"))
        LoggingBits |= (1 << uint32_t(JSpew_Scripts));
    if (strstr(env, "profile"))
        LoggingBits |= (1 << uint32_t(JSpew_Prof));
#ifdef DEBUG
    if (strstr(env, "jsops"))
        LoggingBits |= (1 << uint32_t(JSpew_JSOps));
#endif
    if (strstr(env, "insns"))
        LoggingBits |= (1 << uint32_t(JSpew_Insns) | (1 << uint32_t(JSpew_JSOps)));
    if (strstr(env, "vmframe"))
        LoggingBits |= (1 << uint32_t(JSpew_VMFrame));
    if (strstr(env, "pics"))
        LoggingBits |= (1 << uint32_t(JSpew_PICs));
    if (strstr(env, "slowcalls"))
        LoggingBits |= (1 << uint32_t(JSpew_SlowCalls));
    if (strstr(env, "analysis"))
        LoggingBits |= (1 << uint32_t(JSpew_Analysis));
    if (strstr(env, "regalloc"))
        LoggingBits |= (1 << uint32_t(JSpew_Regalloc));
    if (strstr(env, "recompile"))
        LoggingBits |= (1 << uint32_t(JSpew_Recompile));
    if (strstr(env, "inlin"))
        LoggingBits |= (1 << uint32_t(JSpew_Inlining));
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
    return !!(LoggingBits & (1 << uint32_t(channel)));
}

void
js::JaegerSpew(JaegerSpewChannel channel, const char *fmt, ...)
{
    JS_ASSERT(LoggingChecked);

    if (!(LoggingBits & (1 << uint32_t(channel))))
        return;

    fprintf(stderr, "[jaeger] %-7s  ", ChannelNames[channel]);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    /* fprintf(stdout, "\n"); */
}

#endif

