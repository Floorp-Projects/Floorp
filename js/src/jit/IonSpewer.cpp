/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG

#include "jit/IonSpewer.h"

#include "jit/Ion.h"
#include "jit/MIR.h"

#include "vm/HelperThreads.h"

#ifndef ION_SPEW_DIR
# if defined(_WIN32)
#  define ION_SPEW_DIR ""
# elif defined(__ANDROID__)
#  define ION_SPEW_DIR "/data/local/tmp/"
# else
#  define ION_SPEW_DIR "/tmp/"
# endif
#endif

using namespace js;
using namespace js::jit;

// IonSpewer singleton.
static IonSpewer ionspewer;

static bool LoggingChecked = false;
static uint32_t LoggingBits = 0;
static uint32_t filteredOutCompilations = 0;

static const char * const ChannelNames[] =
{
#define IONSPEW_CHANNEL(name) #name,
    IONSPEW_CHANNEL_LIST(IONSPEW_CHANNEL)
#undef IONSPEW_CHANNEL
};

static bool
FilterContainsLocation(HandleScript function)
{
    static const char *filter = getenv("IONFILTER");

    // If there is no filter we accept all outputs.
    if (!filter || !filter[0])
        return true;

    // Disable asm.js output when filter is set.
    if (!function)
        return false;

    const char *filename = function->filename();
    const size_t line = function->lineno();
    const size_t filelen = strlen(filename);
    const char *index = strstr(filter, filename);
    while (index) {
        if (index == filter || index[-1] == ',') {
            if (index[filelen] == 0 || index[filelen] == ',')
                return true;
            if (index[filelen] == ':' && line != size_t(-1)) {
                size_t read_line = strtoul(&index[filelen + 1], nullptr, 10);
                if (read_line == line)
                    return true;
            }
        }
        index = strstr(index + filelen, filename);
    }
    return false;
}

void
jit::EnableIonDebugLogging()
{
    EnableChannel(IonSpew_Logs);
    ionspewer.init();
}

void
jit::IonSpewNewFunction(MIRGraph *graph, HandleScript func)
{
    if (GetIonContext()->runtime->onMainThread())
        ionspewer.beginFunction(graph, func);
}

void
jit::IonSpewPass(const char *pass)
{
    if (GetIonContext()->runtime->onMainThread())
        ionspewer.spewPass(pass);
}

void
jit::IonSpewPass(const char *pass, LinearScanAllocator *ra)
{
    if (GetIonContext()->runtime->onMainThread())
        ionspewer.spewPass(pass, ra);
}

void
jit::IonSpewEndFunction()
{
    if (GetIonContext()->runtime->onMainThread())
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

bool
IonSpewer::isSpewingFunction() const
{
    return inited_ && graph;
}

void
IonSpewer::beginFunction(MIRGraph *graph, HandleScript function)
{
    if (!inited_)
        return;

    if (!FilterContainsLocation(function)) {
        JS_ASSERT(!this->graph);
        // filter out logs during the compilation.
        filteredOutCompilations++;
        return;
    }

    this->graph = graph;

    c1Spewer.beginFunction(graph, function);
    jsonSpewer.beginFunction(function);
}

void
IonSpewer::spewPass(const char *pass)
{
    if (!isSpewingFunction())
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
    if (!isSpewingFunction())
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
    if (!isSpewingFunction()) {
        if (inited_) {
            JS_ASSERT(filteredOutCompilations != 0);
            filteredOutCompilations--;
        }
        return;
    }

    c1Spewer.endFunction();
    jsonSpewer.endFunction();

    this->graph = nullptr;
}


FILE *jit::IonSpewFile = nullptr;

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
jit::CheckLogging()
{
    if (LoggingChecked)
        return;
    LoggingChecked = true;
    const char *env = getenv("IONFLAGS");
    if (!env)
        return;
    if (strstr(env, "help")) {
        fflush(nullptr);
        printf(
            "\n"
            "usage: IONFLAGS=option,option,option,... where options can be:\n"
            "\n"
            "  aborts     Compilation abort messages\n"
            "  scripts    Compiled scripts\n"
            "  mir        MIR information\n"
            "  escape     Escape analysis\n"
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
            "  range      Range Analysis\n"
            "  unroll     Loop unrolling\n"
            "  logs       C1 and JSON visualization logging\n"
            "  profiling  Profiling-related information\n"
            "  all        Everything\n"
            "\n"
            "  bl-aborts  Baseline compiler abort messages\n"
            "  bl-scripts Baseline script-compilation\n"
            "  bl-op      Baseline compiler detailed op-specific messages\n"
            "  bl-ic      Baseline inline-cache messages\n"
            "  bl-ic-fb   Baseline IC fallback stub messages\n"
            "  bl-osr     Baseline IC OSR messages\n"
            "  bl-bails   Baseline bailouts\n"
            "  bl-dbg-osr Baseline debug mode on stack recompile messages\n"
            "  bl-all     All baseline spew\n"
            "\n"
        );
        exit(0);
        /*NOTREACHED*/
    }
    if (ContainsFlag(env, "aborts"))
        EnableChannel(IonSpew_Abort);
    if (ContainsFlag(env, "escape"))
        EnableChannel(IonSpew_Escape);
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
    if (ContainsFlag(env, "unroll"))
        EnableChannel(IonSpew_Unrolling);
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
    if (ContainsFlag(env, "profiling"))
        EnableChannel(IonSpew_Profiling);
    if (ContainsFlag(env, "all"))
        LoggingBits = uint32_t(-1);

    if (ContainsFlag(env, "bl-aborts"))
        EnableChannel(IonSpew_BaselineAbort);
    if (ContainsFlag(env, "bl-scripts"))
        EnableChannel(IonSpew_BaselineScripts);
    if (ContainsFlag(env, "bl-op"))
        EnableChannel(IonSpew_BaselineOp);
    if (ContainsFlag(env, "bl-ic"))
        EnableChannel(IonSpew_BaselineIC);
    if (ContainsFlag(env, "bl-ic-fb"))
        EnableChannel(IonSpew_BaselineICFallback);
    if (ContainsFlag(env, "bl-osr"))
        EnableChannel(IonSpew_BaselineOSR);
    if (ContainsFlag(env, "bl-bails"))
        EnableChannel(IonSpew_BaselineBailouts);
    if (ContainsFlag(env, "bl-dbg-osr"))
        EnableChannel(IonSpew_BaselineDebugModeOSR);
    if (ContainsFlag(env, "bl-all")) {
        EnableChannel(IonSpew_BaselineAbort);
        EnableChannel(IonSpew_BaselineScripts);
        EnableChannel(IonSpew_BaselineOp);
        EnableChannel(IonSpew_BaselineIC);
        EnableChannel(IonSpew_BaselineICFallback);
        EnableChannel(IonSpew_BaselineOSR);
        EnableChannel(IonSpew_BaselineBailouts);
        EnableChannel(IonSpew_BaselineDebugModeOSR);
    }

    IonSpewFile = stderr;
}

void
jit::IonSpewStartVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    if (!IonSpewEnabled(channel))
        return;

    IonSpewHeader(channel);
    vfprintf(stderr, fmt, ap);
}

void
jit::IonSpewContVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    if (!IonSpewEnabled(channel))
        return;

    vfprintf(stderr, fmt, ap);
}

void
jit::IonSpewFin(IonSpewChannel channel)
{
    if (!IonSpewEnabled(channel))
        return;

    fprintf(stderr, "\n");
}

void
jit::IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    IonSpewStartVA(channel, fmt, ap);
    IonSpewFin(channel);
}

void
jit::IonSpew(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewVA(channel, fmt, ap);
    va_end(ap);
}

void
jit::IonSpewDef(IonSpewChannel channel, const char *str, MDefinition *def)
{
    if (!IonSpewEnabled(channel))
        return;

    IonSpewHeader(channel);
    fprintf(IonSpewFile, "%s", str);
    def->dump(IonSpewFile);
    def->dumpLocation(IonSpewFile);
}

void
jit::IonSpewStart(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewStartVA(channel, fmt, ap);
    va_end(ap);
}
void
jit::IonSpewCont(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewContVA(channel, fmt, ap);
    va_end(ap);
}

void
jit::IonSpewHeader(IonSpewChannel channel)
{
    if (!IonSpewEnabled(channel))
        return;

    fprintf(stderr, "[%s] ", ChannelNames[channel]);
}

bool
jit::IonSpewEnabled(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    return (LoggingBits & (1 << uint32_t(channel))) && !filteredOutCompilations;
}

void
jit::EnableChannel(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    LoggingBits |= (1 << uint32_t(channel));
}

void
jit::DisableChannel(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);
    LoggingBits &= ~(1 << uint32_t(channel));
}

IonSpewFunction::IonSpewFunction(MIRGraph *graph, JS::HandleScript function)
{
    IonSpewNewFunction(graph, function);
}

IonSpewFunction::~IonSpewFunction()
{
    IonSpewEndFunction();
}

#endif /* DEBUG */

