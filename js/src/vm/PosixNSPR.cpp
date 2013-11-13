/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/PosixNSPR.h"

#include "js/Utility.h"

#ifdef JS_POSIX_NSPR

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif

class nspr::Thread
{
    pthread_t pthread_;
    void (*start)(void *arg);
    void *arg;
    bool joinable;

  public:
    Thread(void (*start)(void *arg), void *arg, bool joinable)
      : start(start), arg(arg), joinable(joinable) {}

    static void *ThreadRoutine(void *arg);

    pthread_t &pthread() { return pthread_; }
};

static pthread_key_t gSelfThreadIndex;
static nspr::Thread gMainThread(nullptr, nullptr, false);

void *
nspr::Thread::ThreadRoutine(void *arg)
{
    Thread *self = static_cast<Thread *>(arg);
    pthread_setspecific(gSelfThreadIndex, self);
    self->start(self->arg);
    if (!self->joinable)
        js_delete(self);
    return nullptr;
}

static bool gInitialized;

void
DummyDestructor(void *)
{
}

/* Should be called from the main thread. */
static void
Initialize()
{
    gInitialized = true;

    if (pthread_key_create(&gSelfThreadIndex, DummyDestructor)) {
        MOZ_CRASH();
        return;
    }

    pthread_setspecific(gSelfThreadIndex, &gMainThread);
}

PRThread *
PR_CreateThread(PRThreadType type,
		void (*start)(void *arg),
		void *arg,
		PRThreadPriority priority,
		PRThreadScope scope,
		PRThreadState state,
		uint32_t stackSize)
{
    JS_ASSERT(type == PR_USER_THREAD);
    JS_ASSERT(priority == PR_PRIORITY_NORMAL);

    if (!gInitialized) {
        /*
         * We assume that the first call to PR_CreateThread happens on the main
         * thread.
         */
        Initialize();
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr))
        return nullptr;

    if (stackSize && pthread_attr_setstacksize(&attr, stackSize)) {
        pthread_attr_destroy(&attr);
        return nullptr;
    }

    nspr::Thread *t = js_new<nspr::Thread>(start, arg,
                                           state != PR_UNJOINABLE_THREAD);
    if (!t) {
        pthread_attr_destroy(&attr);
        return nullptr;
    }

    if (pthread_create(&t->pthread(), &attr, &nspr::Thread::ThreadRoutine, t)) {
        pthread_attr_destroy(&attr);
        js_delete(t);
        return nullptr;
    }

    if (state == PR_UNJOINABLE_THREAD) {
        if (pthread_detach(t->pthread())) {
            pthread_attr_destroy(&attr);
            js_delete(t);
            return nullptr;
        }
    }

    pthread_attr_destroy(&attr);

    return t;
}

PRStatus
PR_JoinThread(PRThread *thread)
{
    if (pthread_join(thread->pthread(), nullptr))
        return PR_FAILURE;

    js_delete(thread);

    return PR_SUCCESS;
}

PRThread *
PR_GetCurrentThread()
{
    if (!gInitialized)
        Initialize();

    return (PRThread *)pthread_getspecific(gSelfThreadIndex);
}

PRStatus
PR_SetCurrentThreadName(const char *name)
{
    int result;
#ifdef XP_MACOSX
    result = pthread_setname_np(name);
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), name);
    result = 0;
#elif defined(__NetBSD__)
    result = pthread_setname_np(pthread_self(), "%s", (void *)name);
#else
    result = pthread_setname_np(pthread_self(), name);
#endif
    if (result)
        return PR_FAILURE;
    return PR_SUCCESS;
}

static const size_t MaxTLSKeyCount = 32;
static size_t gTLSKeyCount;
static pthread_key_t gTLSKeys[MaxTLSKeyCount];

PRStatus
PR_NewThreadPrivateIndex(unsigned *newIndex, PRThreadPrivateDTOR destructor)
{
    /*
     * We only call PR_NewThreadPrivateIndex from the main thread, so there's no
     * need to lock the table of TLS keys.
     */
    JS_ASSERT(PR_GetCurrentThread() == &gMainThread);

    pthread_key_t key;
    if (pthread_key_create(&key, destructor))
        return PR_FAILURE;

    JS_ASSERT(gTLSKeyCount + 1 < MaxTLSKeyCount);

    gTLSKeys[gTLSKeyCount] = key;
    *newIndex = gTLSKeyCount;
    gTLSKeyCount++;

    return PR_SUCCESS;
}

PRStatus
PR_SetThreadPrivate(unsigned index, void *priv)
{
    if (index >= gTLSKeyCount)
        return PR_FAILURE;
    if (pthread_setspecific(gTLSKeys[index], priv))
        return PR_FAILURE;
    return PR_SUCCESS;
}

void *
PR_GetThreadPrivate(unsigned index)
{
    if (index >= gTLSKeyCount)
        return nullptr;
    return pthread_getspecific(gTLSKeys[index]);
}

PRStatus
PR_CallOnce(PRCallOnceType *once, PRCallOnceFN func)
{
    MOZ_ASSUME_UNREACHABLE("PR_CallOnce unimplemented");
}

PRStatus
PR_CallOnceWithArg(PRCallOnceType *once, PRCallOnceWithArgFN func, void *arg)
{
    MOZ_ASSUME_UNREACHABLE("PR_CallOnceWithArg unimplemented");
}

class nspr::Lock
{
    pthread_mutex_t mutex_;

  public:
    Lock() {}
    pthread_mutex_t &mutex() { return mutex_; }
};

PRLock *
PR_NewLock()
{
    nspr::Lock *lock = js_new<nspr::Lock>();
    if (!lock)
        return nullptr;

    if (pthread_mutex_init(&lock->mutex(), nullptr)) {
        js_delete(lock);
        return nullptr;
    }

    return lock;
}

void
PR_DestroyLock(PRLock *lock)
{
    pthread_mutex_destroy(&lock->mutex());
    js_delete(lock);
}

void
PR_Lock(PRLock *lock)
{
    pthread_mutex_lock(&lock->mutex());
}

PRStatus
PR_Unlock(PRLock *lock)
{
    if (pthread_mutex_unlock(&lock->mutex()))
        return PR_FAILURE;
    return PR_SUCCESS;
}

class nspr::CondVar
{
    pthread_cond_t cond_;
    nspr::Lock *lock_;

  public:
    CondVar(nspr::Lock *lock) : lock_(lock) {}
    pthread_cond_t &cond() { return cond_; }
    nspr::Lock *lock() { return lock_; }
};

PRCondVar *
PR_NewCondVar(PRLock *lock)
{
    nspr::CondVar *cvar = js_new<nspr::CondVar>(lock);
    if (!cvar)
        return nullptr;

    if (pthread_cond_init(&cvar->cond(), nullptr)) {
        js_delete(cvar);
        return nullptr;
    }

    return cvar;
}

void
PR_DestroyCondVar(PRCondVar *cvar)
{
    pthread_cond_destroy(&cvar->cond());
    js_delete(cvar);
}

PRStatus
PR_NotifyCondVar(PRCondVar *cvar)
{
    if (pthread_cond_signal(&cvar->cond()))
        return PR_FAILURE;
    return PR_SUCCESS;
}

PRStatus
PR_NotifyAllCondVar(PRCondVar *cvar)
{
    if (pthread_cond_broadcast(&cvar->cond()))
        return PR_FAILURE;
    return PR_SUCCESS;
}

uint32_t
PR_MillisecondsToInterval(uint32_t milli)
{
    return milli;
}

static const uint64_t TicksPerSecond = 1000;
static const uint64_t NanoSecondsInSeconds = 1000000000;
static const uint64_t MicroSecondsInSeconds = 1000000;

uint32_t
PR_TicksPerSecond()
{
    return TicksPerSecond;
}

PRStatus
PR_WaitCondVar(PRCondVar *cvar, uint32_t timeout)
{
    if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        if (pthread_cond_wait(&cvar->cond(), &cvar->lock()->mutex()))
            return PR_FAILURE;
        return PR_SUCCESS;
    } else {
        struct timespec ts;
        struct timeval tv;

        gettimeofday(&tv, nullptr);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * (NanoSecondsInSeconds / MicroSecondsInSeconds);

        ts.tv_nsec += timeout * (NanoSecondsInSeconds / TicksPerSecond);
        while (uint64_t(ts.tv_nsec) >= NanoSecondsInSeconds) {
            ts.tv_nsec -= NanoSecondsInSeconds;
            ts.tv_sec++;
        }

        int result = pthread_cond_timedwait(&cvar->cond(), &cvar->lock()->mutex(), &ts);
        if (result == 0 || result == ETIMEDOUT)
            return PR_SUCCESS;
        return PR_FAILURE;
    }
}

#endif /* JS_POSIX_NSPR */
