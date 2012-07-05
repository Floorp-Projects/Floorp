/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workers_h__
#define mozilla_dom_workers_workers_h__

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/StandardInteger.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsStringGlue.h"

#define BEGIN_WORKERS_NAMESPACE \
  namespace mozilla { namespace dom { namespace workers {
#define END_WORKERS_NAMESPACE \
  } /* namespace workers */ } /* namespace dom */ } /* namespace mozilla */
#define USING_WORKERS_NAMESPACE \
  using namespace mozilla::dom::workers;

#define WORKERS_SHUTDOWN_TOPIC "web-workers-shutdown"

class nsPIDOMWindow;

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

struct PrivatizableBase
{ };

#ifdef DEBUG
void
AssertIsOnMainThread();
#else
inline void
AssertIsOnMainThread()
{ }
#endif

// All of these are implemented in RuntimeService.cpp
JSBool
ResolveWorkerClasses(JSContext* aCx, JSHandleObject aObj, JSHandleId aId, unsigned aFlags,
                     JSMutableHandleObject aObjp);

void
CancelWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow);

void
SuspendWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow);

void
ResumeWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow);

class WorkerTask {
public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerTask)

    virtual ~WorkerTask() { }

    virtual bool RunTask(JSContext* aCx) = 0;
};

class WorkerCrossThreadDispatcher {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerCrossThreadDispatcher)

  WorkerCrossThreadDispatcher(WorkerPrivate* aPrivate) :
    mMutex("WorkerCrossThreadDispatcher"), mPrivate(aPrivate) {}
  void Forget()
  {
    mozilla::MutexAutoLock lock(mMutex);
    mPrivate = nsnull;
  }

  /**
   * Generically useful function for running a bit of C++ code on the worker
   * thread.
   */
  bool PostTask(WorkerTask* aTask);

protected:
  friend class WorkerPrivate;

  // Must be acquired *before* the WorkerPrivate's mutex, when they're both held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mPrivate;
};

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, jsval aWorker);

// Random unique constant to facilitate JSPrincipal debugging
const uint32_t kJSPrincipalsDebugToken = 0x7e2df9d2;

namespace exceptions {

// Implemented in Exceptions.cpp
void
ThrowDOMExceptionForNSResult(JSContext* aCx, nsresult aNSResult);

} // namespace exceptions

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workers_h__
