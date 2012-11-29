/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG

#include "Ion.h"
#include "IonSpewer.h"

#ifndef ION_SPEW_DIR
# if defined(_WIN32)
#  define ION_SPEW_DIR ""
# else
#  define ION_SPEW_DIR "/tmp/"
# endif
#endif

using namespace js;
using namespace js::ion;

// IonSpewer singleton.
static IonSpewer ionspewer;

static bool LoggingChecked = false;
static uint32_t LoggingBits = 0;

static const char *ChannelNames[] =
{
#define IONSPEW_CHANNEL(name) #name,
    IONSPEW_CHANNEL_LIST(IONSPEW_CHANNEL)
#undef IONSPEW_CHANNEL
};

void
ion::EnableIonDebugLogging()
{
    ionspewer.init();
}

void
ion::IonSpewNewFunction(MIRGraph *graph, HandleScript function)
{
    if (!js_IonOptions.parallelCompilation)
        ionspewer.beginFunction(graph, function);
}

void
ion::IonSpewPass(const char *pass)
{
    if (!js_IonOptions.parallelCompilation)
        ionspewer.spewPass(pass);
}

void
ion::IonSpewPass(const char *pass, LinearScanAllocator *ra)
{
    if (!js_IonOptions.parallelCompilation)
        ionspewer.spewPass(pass, ra);
}

void
ion::IonSpewEndFunction()
{
    if (!js_IonOptions.parallelCompilation)
        ionspewer.endFunction();
}


IonSpewer::~IonSpewer()
{
    if (!inited_)
        return;

    c1Spewer.finish();
    jsonSpewer.finish();
}

bool
IonSpewer::init()
{
    if (inited_)
        return true;

    if (!c1Spewer.init(ION_SPEW_DIR "ion.cfg"))
        return false;
    if (!jsonSpewer.init(ION_SPEW_DIR "ion.json"))
        return false;

    inited_ = true;
    return true;
}

void
IonSpewer::beginFunction(MIRGraph *graph, HandleScript function)
{
    if (!inited_)
        return;

    this->graph = graph;
    this->function = function;

    c1Spewer.beginFunction(graph, function);
    jsonSpewer.beginFunction(function);
}

void
IonSpewer::spewPass(const char *pass)
{
    if (!inited_)
        return;

    c1Spewer.spewPass(pass);
    jsonSpewer.beginPass(pass);
    jsonSpewer.spewMIR(graph);
    jsonSpewer.spewLIR(graph);
    jsonSpewer.endPass();
}

void
IonSpewer::spewPass(const char *pass, LinearScanAllocator *ra)
{
    if (!inited_)
        return;

    c1Spewer.spewPass(pass);
    c1Spewer.spewIntervals(pass, ra);
    jsonSpewer.beginPass(pass);
    jsonSpewer.spewMIR(graph);
    jsonSpewer.spewLIR(graph);
    jsonSpewer.spewIntervals(ra);
    jsonSpewer.endPass();
}

void
IonSpewer::endFunction()
{
    if (!inited_)
        return;

    c1Spewer.endFunction();
    jsonSpewer.endFunction();
}


FILE *ion::IonSpewFile = NULL;

static bool
ContainsFlag(const char *str, const char *flag)
{
    size_t flaglen = strlen(flag);
    const char *index = strstr(str, flag);
    while (index) {
        if ((index == str || index[-1] == ',') && (index[flaglen] == 0 || index[flaglen] == ','))
            return true;
        index = strstr(index + flaglen, flag);
    }
    return false;
}

void
ion::CheckLogging()
{
    if (LoggingChecked)
        return;
    LoggingChecked = true;
    const char *env = getenv("IONFLAGS");
    if (!env)
        return;
    if (strstr(env, "help")) {
        fflush(NULL);
        printf(
            "\n"
            "usage: IONFLAGS=option,option,option,... where options can be:\n"
            "\n"
            "  aborts     Compilation abort messages\n"
            "  scripts    Compiled scripts\n"
            "  mir        MIR information\n"
            "  alias      Alias analysis\n"
            "  gvn        Global Value Numbering\n"
            "  licm       Loop invariant code motion\n"
            "  regalloc   Register allocation\n"
            "  inline     Inlining\n"
            "  snapshots  Snapshot information\n"
            "  codegen    Native code generation\n"
            "  bailouts   Bailouts\n"
            "  caches     Inline caches\n"
            "  osi        Invalidation\n"
            "  safepoints Safepoints\n"
            "  pools      Literal Pools (ARM only for now)\n"
            "  cacheflush Instruction Cache flushes (ARM only for now)\n"
            "  logs       C1 and JSON visualization logging\n"
            "  all        Everything\n"
            "\n"
        );
        exit(0);
        /*NOTREACHED*/
    }
    if (ContainsFlag(env, "aborts"))
        EnableChannel(IonSpew_Abort);
    if (ContainsFlag(env, "alias"))
        EnableChannel(IonSpew_Alias);
    if (ContainsFlag(env, "scripts"))
        EnableChannel(IonSpew_Scripts);
    if (ContainsFlag(env, "mir"))
        EnableChannel(IonSpew_MIR);
    if (ContainsFlag(env, "gvn"))
        EnableChannel(IonSpew_GVN);
    if (ContainsFlag(env, "range"))
        EnableChannel(IonSpew_Range);
    if (ContainsFlag(env, "licm"))
        EnableChannel(IonSpew_LICM);
    if (ContainsFlag(env, "regalloc"))
        EnableChannel(IonSpew_RegAlloc);
    if (ContainsFlag(env, "inline"))
        EnableChannel(IonSpew_Inlining);
    if (ContainsFlag(env, "snapshots"))
        EnableChannel(IonSpew_Snapshots);
    if (ContainsFlag(env, "codegen"))
        EnableChannel(IonSpew_Codegen);
    if (ContainsFlag(env, "bailouts"))
        EnableChannel(IonSpew_Bailouts);
    if (ContainsFlag(env, "osi"))
        EnableChannel(IonSpew_Invalidate);
    if (ContainsFlag(env, "caches"))
        EnableChannel(IonSpew_InlineCaches);
    if (ContainsFlag(env, "safepoints"))
        EnableChannel(IonSpew_Safepoints);
    if (ContainsFlag(env, "pools"))
        EnableChannel(IonSpew_Pools);
    if (ContainsFlag(env, "cacheflush"))
        EnableChannel(IonSpew_CacheFlush);
    if (ContainsFlag(env, "logs"))
        EnableIonDebugLogging();
    if (ContainsFlag(env, "all"))
        LoggingBits = uint32_t(-1);

    if (LoggingBits != 0)
        EnableIonDebugLogging();

    IonSpewFile = stderr;
}

void
ion::IonSpewStartVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    if (!IonSpewEnabled(channel))
        return;

    IonSpewHeader(channel);
    vfprintf(stderr, fmt, ap);
}

void
ion::IonSpewContVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    if (!IonSpewEnabled(channel))
        return;

    vfprintf(stderr, fmt, ap);
}

void
ion::IonSpewFin(IonSpewChannel channel)
{
    if (!IonSpewEnabled(channel))
        return;

    fprintf(stderr, "\n");
}

void
ion::IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    IonSpewStartVA(channel, fmt, ap);
    IonSpewFin(channel);
}

void
ion::IonSpew(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewVA(channel, fmt, ap);
    va_end(ap);
}

void
ion::IonSpewStart(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewStartVA(channel, fmt, ap);
    va_end(ap);
}
void
ion::IonSpewCont(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewContVA(channel, fmt, ap);
    va_end(ap);
}

void
ion::IonSpewHeader(IonSpewChannel channel)
{
    if (!IonSpewEnabled(channel))
        return;

    fprintf(stderr, "[%s] ", ChannelNames[channel]);

}

bool
ion::IonSpewEnabled(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    return LoggingBits & (1 << uint32_t(channel));
}

void
ion::EnableChannel(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    LoggingBits |= (1 << uint32_t(channel));
}

void
ion::DisableChannel(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    LoggingBits &= ~(1 << uint32_t(channel));
}

#endif /* DEBUG */

