/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsyncInterrupt.h"

#include "jit/JitCompartment.h"
#include "util/Windows.h"

#if defined(ANDROID)
# include <sys/system_properties.h>
#endif

using namespace js;
using namespace js::jit;

using mozilla::PodArrayZero;

static void
RedirectIonBackedgesToInterruptCheck(JSContext* cx)
{
    // Jitcode may only be modified on the runtime's active thread.
    if (cx != cx->runtime()->activeContext())
        return;

    // The faulting thread is suspended so we can access cx fields that can
    // normally only be accessed by the cx's active thread.
    AutoNoteSingleThreadedRegion anstr;

    Zone* zone = cx->zoneRaw();
    if (zone && !zone->isAtomsZone()) {
        jit::JitRuntime* jitRuntime = cx->runtime()->jitRuntime();
        if (!jitRuntime)
            return;

        // If the backedge list is being mutated, the pc must be in C++ code and
        // thus not in a JIT iloop. We assume that the interrupt flag will be
        // checked at least once before entering JIT code (if not, no big deal;
        // the browser will just request another interrupt in a second).
        if (!jitRuntime->preventBackedgePatching()) {
            jit::JitZoneGroup* jzg = zone->group()->jitZoneGroup;
            jzg->patchIonBackedges(cx, jit::JitZoneGroup::BackedgeInterruptCheck);
        }
    }
}

#if !defined(XP_WIN)
// For the interrupt signal, pick a signal number that:
//  - is not otherwise used by mozilla or standard libraries
//  - defaults to nostop and noprint on gdb/lldb so that noone is bothered
// SIGVTALRM a relative of SIGALRM, so intended for user code, but, unlike
// SIGALRM, not used anywhere else in Mozilla.
static const int sJitAsyncInterruptSignal = SIGVTALRM;

static void
JitAsyncInterruptHandler(int signum, siginfo_t*, void*)
{
    MOZ_RELEASE_ASSERT(signum == sJitAsyncInterruptSignal);

    JSContext* cx = TlsContext.get();
    if (!cx)
        return;

#if defined(JS_SIMULATOR_ARM) || defined(JS_SIMULATOR_MIPS32) || defined(JS_SIMULATOR_MIPS64)
    SimulatorProcess::ICacheCheckingDisableCount++;
#endif

    RedirectIonBackedgesToInterruptCheck(cx);

#if defined(JS_SIMULATOR_ARM) || defined(JS_SIMULATOR_MIPS32) || defined(JS_SIMULATOR_MIPS64)
    SimulatorProcess::cacheInvalidatedBySignalHandler_ = true;
    SimulatorProcess::ICacheCheckingDisableCount--;
#endif

    cx->finishHandlingJitInterrupt();
}
#endif

static bool sTriedInstallAsyncInterrupt = false;
static bool sHaveAsyncInterrupt = false;

void
jit::EnsureAsyncInterrupt(JSContext* cx)
{
    // We assume that there are no races creating the first JSRuntime of the process.
    if (sTriedInstallAsyncInterrupt)
        return;
    sTriedInstallAsyncInterrupt = true;

#if defined(ANDROID) && !defined(__aarch64__)
    // Before Android 4.4 (SDK version 19), there is a bug
    //   https://android-review.googlesource.com/#/c/52333
    // in Bionic's pthread_join which causes pthread_join to return early when
    // pthread_kill is used (on any thread). Nobody expects the pthread_cond_wait
    // EINTRquisition.
    char version_string[PROP_VALUE_MAX];
    PodArrayZero(version_string);
    if (__system_property_get("ro.build.version.sdk", version_string) > 0) {
        if (atol(version_string) < 19)
            return;
    }
#endif

#if defined(XP_WIN)
    // Windows uses SuspendThread to stop the active thread from another thread.
#else
    struct sigaction interruptHandler;
    interruptHandler.sa_flags = SA_SIGINFO;
    interruptHandler.sa_sigaction = &JitAsyncInterruptHandler;
    sigemptyset(&interruptHandler.sa_mask);
    struct sigaction prev;
    if (sigaction(sJitAsyncInterruptSignal, &interruptHandler, &prev))
        MOZ_CRASH("unable to install interrupt handler");

    // There shouldn't be any other handlers installed for
    // sJitAsyncInterruptSignal. If there are, we could always forward, but we
    // need to understand what we're doing to avoid problematic interference.
    if ((prev.sa_flags & SA_SIGINFO && prev.sa_sigaction) ||
        (prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN))
    {
        MOZ_CRASH("contention for interrupt signal");
    }
#endif // defined(XP_WIN)

    sHaveAsyncInterrupt = true;
}

bool
jit::HaveAsyncInterrupt()
{
    MOZ_ASSERT(sTriedInstallAsyncInterrupt);
    return sHaveAsyncInterrupt;
}

// JSRuntime::requestInterrupt sets interrupt_ (which is checked frequently by
// C++ code at every Baseline JIT loop backedge) and jitStackLimit_ (which is
// checked at every Baseline and Ion JIT function prologue). The remaining
// sources of potential iloops (Ion loop backedges) are handled by this
// function: Ion loop backedges are patched to instead point to a stub that
// handles the interrupt;
void
jit::InterruptRunningCode(JSContext* cx)
{
    // If signal handlers weren't installed, then Ion emit normal interrupt
    // checks and don't need asynchronous interruption.
    MOZ_ASSERT(sTriedInstallAsyncInterrupt);
    if (!sHaveAsyncInterrupt)
        return;

    // Do nothing if we're already handling an interrupt here, to avoid races
    // below and in JitRuntime::patchIonBackedges.
    if (!cx->startHandlingJitInterrupt())
        return;

    // If we are on context's thread, then we can patch Ion backedges without
    // any special synchronization.
    if (cx == TlsContext.get()) {
        RedirectIonBackedgesToInterruptCheck(cx);
        cx->finishHandlingJitInterrupt();
        return;
    }

    // We are not on the runtime's active thread, so we need to halt the
    // runtime's active thread first.
#if defined(XP_WIN)
    // On Windows, we can simply suspend the active thread. SuspendThread can
    // sporadically fail if the thread is in the middle of a syscall. Rather
    // than retrying in a loop, just wait for the next request for interrupt.
    HANDLE thread = (HANDLE)cx->threadNative();
    if (SuspendThread(thread) != (DWORD)-1) {
        RedirectIonBackedgesToInterruptCheck(cx);
        ResumeThread(thread);
    }
    cx->finishHandlingJitInterrupt();
#else
    // On Unix, we instead deliver an async signal to the active thread which
    // halts the thread and callers our JitAsyncInterruptHandler (which has
    // already been installed by EnsureSignalHandlersInstalled).
    pthread_t thread = (pthread_t)cx->threadNative();
    pthread_kill(thread, sJitAsyncInterruptSignal);
#endif
}

