/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonSpewer_h
#define jit_IonSpewer_h

#include "mozilla/DebugOnly.h"

#include <stdarg.h>

#include "jit/C1Spewer.h"
#include "jit/JSONSpewer.h"

namespace js {
namespace ion {

// New channels may be added below.
#define IONSPEW_CHANNEL_LIST(_)             \
    /* Used to abort SSA construction */    \
    _(Abort)                                \
    /* Information about compiled scripts */\
    _(Scripts)                              \
    /* Information during MIR building */   \
    _(MIR)                                  \
    /* Information during alias analysis */ \
    _(Alias)                                \
    /* Information during GVN */            \
    _(GVN)                                  \
    /* Information during Range analysis */ \
    _(Range)                                \
    /* Information during LICM */           \
    _(LICM)                                 \
    /* Information during regalloc */       \
    _(RegAlloc)                             \
    /* Information during inlining */       \
    _(Inlining)                             \
    /* Information during codegen */        \
    _(Codegen)                              \
    /* Information during bailouts */       \
    _(Bailouts)                             \
    /* Information during OSI */            \
    _(Invalidate)                           \
    /* Debug info about snapshots */        \
    _(Snapshots)                            \
    /* Generated inline cache stubs */      \
    _(InlineCaches)                         \
    /* Debug info about safepoints */       \
    _(Safepoints)                           \
    /* Debug info about Pools*/             \
    _(Pools)                                \
    /* Calls to js::ion::Trace() */         \
    _(Trace)                                \
    /* Debug info about the I$ */           \
    _(CacheFlush)                           \
                                            \
    /* BASELINE COMPILER SPEW */            \
                                            \
    /* Aborting Script Compilation. */      \
    _(BaselineAbort)                        \
    /* Script Compilation. */               \
    _(BaselineScripts)                      \
    /* Detailed op-specific spew. */        \
    _(BaselineOp)                           \
    /* Inline caches. */                    \
    _(BaselineIC)                           \
    /* Inline cache fallbacks. */           \
    _(BaselineICFallback)                   \
    /* OSR from Baseline => Ion. */         \
    _(BaselineOSR)                          \
    /* Bailouts. */                         \
    _(BaselineBailouts)


enum IonSpewChannel {
#define IONSPEW_CHANNEL(name) IonSpew_##name,
    IONSPEW_CHANNEL_LIST(IONSPEW_CHANNEL)
#undef IONSPEW_CHANNEL
    IonSpew_Terminator
};


// The IonSpewer is only available on debug builds.
// None of the global functions have effect on non-debug builds.
static const int NULL_ID = -1;

#ifdef DEBUG

class IonSpewer
{
  private:
    MIRGraph *graph;
    HandleScript function;
    C1Spewer c1Spewer;
    JSONSpewer jsonSpewer;
    bool inited_;

  public:
    IonSpewer()
      : graph(NULL), function(NullPtr()), inited_(false)
    { }

    // File output is terminated safely upon destruction.
    ~IonSpewer();

    bool init();
    void beginFunction(MIRGraph *graph, HandleScript);
    bool isSpewingFunction() const;
    void spewPass(const char *pass);
    void spewPass(const char *pass, LinearScanAllocator *ra);
    void endFunction();
};

void IonSpewNewFunction(MIRGraph *graph, HandleScript function);
void IonSpewPass(const char *pass);
void IonSpewPass(const char *pass, LinearScanAllocator *ra);
void IonSpewEndFunction();

void CheckLogging();
extern FILE *IonSpewFile;
void IonSpew(IonSpewChannel channel, const char *fmt, ...);
void IonSpewStart(IonSpewChannel channel, const char *fmt, ...);
void IonSpewCont(IonSpewChannel channel, const char *fmt, ...);
void IonSpewFin(IonSpewChannel channel);
void IonSpewHeader(IonSpewChannel channel);
bool IonSpewEnabled(IonSpewChannel channel);
void IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap);
void IonSpewStartVA(IonSpewChannel channel, const char *fmt, va_list ap);
void IonSpewContVA(IonSpewChannel channel, const char *fmt, va_list ap);

void EnableChannel(IonSpewChannel channel);
void DisableChannel(IonSpewChannel channel);
void EnableIonDebugLogging();

#else

static inline void IonSpewNewFunction(MIRGraph *graph, HandleScript function)
{ }
static inline void IonSpewPass(const char *pass)
{ }
static inline void IonSpewPass(const char *pass, LinearScanAllocator *ra)
{ }
static inline void IonSpewEndFunction()
{ }

static inline void CheckLogging()
{ }
static FILE *const IonSpewFile = NULL;
static inline void IonSpew(IonSpewChannel, const char *fmt, ...)
{ }
static inline void IonSpewStart(IonSpewChannel channel, const char *fmt, ...)
{ }
static inline void IonSpewCont(IonSpewChannel channel, const char *fmt, ...)
{ }
static inline void IonSpewFin(IonSpewChannel channel)
{ }

static inline void IonSpewHeader(IonSpewChannel channel)
{ }
static inline bool IonSpewEnabled(IonSpewChannel channel)
{ return false; }
static inline void IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap)
{ }

static inline void EnableChannel(IonSpewChannel)
{ }
static inline void DisableChannel(IonSpewChannel)
{ }
static inline void EnableIonDebugLogging()
{ }

#endif /* DEBUG */

template <IonSpewChannel Channel>
class AutoDisableSpew
{
    mozilla::DebugOnly<bool> enabled_;

  public:
    AutoDisableSpew()
      : enabled_(IonSpewEnabled(Channel))
    {
        DisableChannel(Channel);
    }

    ~AutoDisableSpew()
    {
#ifdef DEBUG
        if (enabled_)
            EnableChannel(Channel);
#endif
    }
};

} /* ion */
} /* js */

#endif /* jit_IonSpewer_h */
