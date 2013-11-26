/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PosixNSPR_h
#define vm_PosixNSPR_h

#ifdef JS_POSIX_NSPR

#ifndef JS_THREADSAFE
#error "This file must not be included in non-threadsafe mode"
#endif

#include <pthread.h>
#include <stdint.h>

#define PR_ATOMIC_INCREMENT(val) __sync_add_and_fetch(val, 1)
#define PR_ATOMIC_DECREMENT(val) __sync_sub_and_fetch(val, 1)
#define PR_ATOMIC_SET(val, newval) __sync_lock_test_and_set(val, newval)
#define PR_ATOMIC_ADD(ptr, val) __sync_add_and_fetch(ptr, val)

namespace nspr {
class Thread;
class Lock;
class CondVar;
};

typedef nspr::Thread PRThread;
typedef nspr::Lock PRLock;
typedef nspr::CondVar PRCondVar;

enum PRThreadType {
   PR_USER_THREAD,
   PR_SYSTEM_THREAD
};

enum PRThreadPriority
{
   PR_PRIORITY_FIRST   = 0,
   PR_PRIORITY_LOW     = 0,
   PR_PRIORITY_NORMAL  = 1,
   PR_PRIORITY_HIGH    = 2,
   PR_PRIORITY_URGENT  = 3,
   PR_PRIORITY_LAST    = 3
};

enum PRThreadScope {
   PR_LOCAL_THREAD,
   PR_GLOBAL_THREAD,
   PR_GLOBAL_BOUND_THREAD
};

enum PRThreadState {
   PR_JOINABLE_THREAD,
   PR_UNJOINABLE_THREAD
};

PRThread *
PR_CreateThread(PRThreadType type,
                void (*start)(void *arg),
                void *arg,
                PRThreadPriority priority,
                PRThreadScope scope,
                PRThreadState state,
                uint32_t stackSize);

typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

PRStatus
PR_JoinThread(PRThread *thread);

PRThread *
PR_GetCurrentThread();

PRStatus
PR_SetCurrentThreadName(const char *name);

typedef void (*PRThreadPrivateDTOR)(void *priv);

PRStatus
PR_NewThreadPrivateIndex(unsigned *newIndex, PRThreadPrivateDTOR destructor);

PRStatus
PR_SetThreadPrivate(unsigned index, void *priv);

void *
PR_GetThreadPrivate(unsigned index);

struct PRCallOnceType {
    int initialized;
    int32_t inProgress;
    PRStatus status;
};

typedef PRStatus (*PRCallOnceFN)();

PRStatus
PR_CallOnce(PRCallOnceType *once, PRCallOnceFN func);

typedef PRStatus (*PRCallOnceWithArgFN)(void *);

PRStatus
PR_CallOnceWithArg(PRCallOnceType *once, PRCallOnceWithArgFN func, void *arg);

PRLock *
PR_NewLock();

void
PR_DestroyLock(PRLock *lock);

void
PR_Lock(PRLock *lock);

PRStatus
PR_Unlock(PRLock *lock);

PRCondVar *
PR_NewCondVar(PRLock *lock);

void
PR_DestroyCondVar(PRCondVar *cvar);

PRStatus
PR_NotifyCondVar(PRCondVar *cvar);

PRStatus
PR_NotifyAllCondVar(PRCondVar *cvar);

#define PR_INTERVAL_MIN 1000UL
#define PR_INTERVAL_MAX 100000UL

#define PR_INTERVAL_NO_WAIT 0UL
#define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL

uint32_t
PR_MillisecondsToInterval(uint32_t milli);

uint32_t
PR_TicksPerSecond();

PRStatus
PR_WaitCondVar(PRCondVar *cvar, uint32_t timeout);

#endif /* JS_POSIX_NSPR */

#endif /* vm_PosixNSPR_h */
