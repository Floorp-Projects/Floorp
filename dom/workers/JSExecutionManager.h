/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_jsexecutionmanager_h__
#define mozilla_dom_workers_jsexecutionmanager_h__

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerStatus.h"
#include "mozilla/StaticPtr.h"

#include "nsICancelableRunnable.h"

#include "mozilla/Atomics.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h" /* nsRunnable */

#include <deque>

struct JSContext;
class nsIEventTarget;
class nsIGlobalObject;

// The code in this file is responsible for throttling JS execution. It does
// this by introducing a JSExecutionManager class. An execution manager may be
// applied to any number of worker threads or a DocGroup on the main thread.
//
// JS environments associated with a JS execution manager may only execute on a
// certain amount of CPU cores in parallel.
//
// Whenever the main thread, or a worker thread begins executing JS it should
// make sure AutoRequestJSThreadExecution is present on the stack, in practice
// this is done by it being part of AutoEntryScript.
//
// Whenever the main thread may end up blocking on the activity of a worker
// thread, it should make sure to have an AutoYieldJSThreadExecution object
// on the stack.
//
// Whenever a worker thread may end up blocking on the main thread or the
// activity of another worker thread, it should make sure to have an
// AutoYieldJSThreadExecution object on the stack.
//
// Failure to do this may result in a deadlock. When a deadlock occurs due
// to these circumstances we will crash after 20 seconds.
//
// For the main thread this class should only be used in the case of an
// emergency surrounding exploitability of SharedArrayBuffers. A single
// execution manager will then be shared between all Workers and the main
// thread doc group containing the SharedArrayBuffer and ensure all this code
// only runs in a serialized manner. On the main thread we therefore may have
// 1 execution manager per DocGroup, as this is the granularity at which
// SharedArrayBuffers may be present.

namespace mozilla {

class ErrorResult;

namespace dom {

class AutoRequestJSThreadExecution;
class AutoYieldJSThreadExecution;

// This class is used to regulate JS execution when for whatever reason we wish
// to throttle execution of multiple JS environments to occur with a certain
// maximum of synchronously executing threads. This should be used through
// the stack helper classes.
class JSExecutionManager {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSExecutionManager)

  explicit JSExecutionManager(int32_t aMaxRunning = 1)
      : mMaxRunning(aMaxRunning) {}

  enum class RequestState { Granted, ExecutingAlready };

 private:
  friend class AutoRequestJSThreadExecution;
  friend class AutoYieldJSThreadExecution;
  ~JSExecutionManager() = default;

  // Methods used by Auto*JSThreadExecution

  // Request execution permission, returns ExecutingAlready if execution was
  // already granted or does not apply to this thread.
  RequestState RequestJSThreadExecution();

  // Yield JS execution, this asserts that permission is actually granted.
  void YieldJSThreadExecution();

  // Yield JS execution if permission was granted. This returns false if no
  // permission is granted. This method is needed because an execution manager
  // may have been set in between the ctor and dtor of
  // AutoYieldJSThreadExecution.
  bool YieldJSThreadExecutionIfGranted();

  RequestState RequestJSThreadExecutionMainThread();

  // Execution manager currently managing the main thread.
  // MainThread access only.
  static JSExecutionManager* mCurrentMTManager;

  // Workers waiting to be given permission for execution.
  // Guarded by mExecutionQueueMutex.
  std::deque<WorkerPrivate*> mExecutionQueue;

  // Number of threads currently executing concurrently for this manager.
  // Guarded by mExecutionQueueMutex.
  int32_t mRunning = 0;

  // Number of threads allowed to run concurrently for environments managed
  // by this manager.
  // Guarded by mExecutionQueueMutex.
  int32_t mMaxRunning = 1;

  // Mutex that guards the execution queue and associated state.
  Mutex mExecutionQueueMutex =
      Mutex{"JSExecutionManager::sExecutionQueueMutex"};

  // ConditionVariables that blocked threads wait for.
  CondVar mExecutionQueueCondVar =
      CondVar{mExecutionQueueMutex, "JSExecutionManager::sExecutionQueueMutex"};

  // Whether the main thread is currently executing for this manager.
  // MainThread access only.
  bool mMainThreadIsExecuting = false;

  // Whether the main thread is currently awaiting permission to execute. Main
  // thread execution is always prioritized.
  // Guarded by mExecutionQueueMutex.
  bool mMainThreadAwaitingExecution = false;
};

// Helper for managing execution requests and allowing re-entrant permission
// requests.
class MOZ_STACK_CLASS AutoRequestJSThreadExecution {
 public:
  explicit AutoRequestJSThreadExecution(nsIGlobalObject* aGlobalObject,
                                        bool aIsMainThread);

  ~AutoRequestJSThreadExecution() {
    if (mExecutionGrantingManager) {
      mExecutionGrantingManager->YieldJSThreadExecution();
    }
    if (mIsMainThread) {
      if (mOldGrantingManager) {
        mOldGrantingManager->RequestJSThreadExecution();
      }
      JSExecutionManager::mCurrentMTManager = mOldGrantingManager;
    }
  }

 private:
  // The manager we obtained permission from. nullptr if permission was already
  // granted.
  RefPtr<JSExecutionManager> mExecutionGrantingManager;
  // The manager we had permission from before, and where permission should be
  // re-requested upon destruction.
  RefPtr<JSExecutionManager> mOldGrantingManager;

  // We store this for performance reasons.
  bool mIsMainThread;
};

// Class used to wrap code which essentially exits JS execution and may block
// on other threads.
class MOZ_STACK_CLASS AutoYieldJSThreadExecution {
 public:
  AutoYieldJSThreadExecution();

  ~AutoYieldJSThreadExecution() {
    if (mExecutionGrantingManager) {
      mExecutionGrantingManager->RequestJSThreadExecution();
      if (NS_IsMainThread()) {
        JSExecutionManager::mCurrentMTManager = mExecutionGrantingManager;
      }
    }
  }

 private:
  // Set to the granting manager if we were granted permission here.
  RefPtr<JSExecutionManager> mExecutionGrantingManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_jsexecutionmanager_h__
