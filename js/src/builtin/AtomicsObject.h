/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_AtomicsObject_h
#define builtin_AtomicsObject_h

#include "jslock.h"
#include "jsobj.h"

namespace js {

class AtomicsObject : public JSObject
{
  public:
    static const Class class_;
    static JSObject* initClass(JSContext* cx, Handle<GlobalObject*> global);
    static MOZ_MUST_USE bool toString(JSContext* cx, unsigned int argc, Value* vp);
};

MOZ_MUST_USE bool atomics_compareExchange(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_exchange(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_load(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_store(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_add(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_sub(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_and(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_or(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_xor(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_isLockFree(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_wait(JSContext* cx, unsigned argc, Value* vp);
MOZ_MUST_USE bool atomics_wake(JSContext* cx, unsigned argc, Value* vp);

/* asm.js callouts */
namespace wasm { class Instance; }
int32_t atomics_add_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);
int32_t atomics_sub_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);
int32_t atomics_and_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);
int32_t atomics_or_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);
int32_t atomics_xor_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);
int32_t atomics_cmpxchg_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t oldval, int32_t newval);
int32_t atomics_xchg_asm_callout(wasm::Instance* i, int32_t vt, int32_t offset, int32_t value);

class FutexRuntime
{
public:
    static MOZ_MUST_USE bool initialize();
    static void destroy();

    static void lock();
    static void unlock();

    FutexRuntime();
    MOZ_MUST_USE bool initInstance();
    void destroyInstance();

    // Parameters to wake().
    enum WakeReason {
        WakeExplicit,           // Being asked to wake up by another thread
        WakeForJSInterrupt      // Interrupt requested
    };

    // Result code from wait().
    enum WaitResult {
        FutexOK,
        FutexTimedOut
    };

    // Block the calling thread and wait.
    //
    // The futex lock must be held around this call.
    //
    // The timeout is the number of milliseconds, with fractional
    // times allowed; specify positive infinity for an indefinite wait.
    //
    // wait() will not wake up spuriously.  It will return true and
    // set *result to a return code appropriate for
    // Atomics.wait() on success, and return false on error.
    MOZ_MUST_USE bool wait(JSContext* cx, double timeout, WaitResult* result);

    // Wake the thread represented by this Runtime.
    //
    // The futex lock must be held around this call.  (The sleeping
    // thread will not wake up until the caller of Atomics.wake()
    // releases the lock.)
    //
    // If the thread is not waiting then this method does nothing.
    //
    // If the thread is waiting in a call to wait() and the
    // reason is WakeExplicit then the wait() call will return
    // with Woken.
    //
    // If the thread is waiting in a call to wait() and the
    // reason is WakeForJSInterrupt then the wait() will return
    // with WaitingNotifiedForInterrupt; in the latter case the caller
    // of wait() must handle the interrupt.
    void wake(WakeReason reason);

    bool isWaiting();

    // If canWait() returns false (the default) then wait() is disabled
    // on the runtime to which the FutexRuntime belongs.
    bool canWait() {
        return canWait_;
    }

    void setCanWait(bool flag) {
        canWait_ = flag;
    }

  private:
    enum FutexState {
        Idle,                        // We are not waiting or woken
        Waiting,                     // We are waiting, nothing has happened yet
        WaitingNotifiedForInterrupt, // We are waiting, but have been interrupted,
                                     //   and have not yet started running the
                                     //   interrupt handler
        WaitingInterrupted,          // We are waiting, but have been interrupted
                                     //   and are running the interrupt handler
        Woken                        // Woken by a script call to Atomics.wake
    };

    // Condition variable that this runtime will wait on.
    PRCondVar* cond_;

    // Current futex state for this runtime.  When not in a wait this
    // is Idle; when in a wait it is Waiting or the reason the futex
    // is about to wake up.
    FutexState state_;

    // Shared futex lock for all runtimes.  We can perhaps do better,
    // but any lock will need to be per-domain (consider SharedWorker)
    // or coarser.
    static mozilla::Atomic<PRLock*> lock_;

#ifdef DEBUG
    // Null or the thread holding the lock.
    static mozilla::Atomic<PRThread*> lockHolder_;
#endif

    // A flag that controls whether waiting is allowed.
    bool canWait_;
};

JSObject*
InitAtomicsClass(JSContext* cx, HandleObject obj);

}  /* namespace js */

#endif /* builtin_AtomicsObject_h */
