/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_worklet_WorkletThread_h
#define mozilla_dom_worklet_WorkletThread_h

#include "mozilla/Attributes.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/TimeStamp.h"
#include "nsThread.h"

class nsIRunnable;

namespace mozilla {
namespace dom {

class WorkletThread final : public nsThread, public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  static already_AddRefed<WorkletThread>
  Create(const WorkletLoadInfo& aWorkletLoadInfo);

  static WorkletThread*
  Get();

  static bool
  IsOnWorkletThread();

  static void
  AssertIsOnWorkletThread();

  JSContext*
  GetJSContext() const;

  const WorkletLoadInfo&
  GetWorkletLoadInfo() const;

  nsresult
  DispatchRunnable(already_AddRefed<nsIRunnable> aRunnable);

  void
  Terminate();

  DOMHighResTimeStamp
  TimeStampToDOMHighRes(const TimeStamp& aTimeStamp) const
  {
    MOZ_ASSERT(!aTimeStamp.IsNull());
    TimeDuration duration = aTimeStamp - mCreationTimeStamp;
    return duration.ToMilliseconds();
  }

private:
  explicit WorkletThread(const WorkletLoadInfo& aWorkletLoadInfo);
  ~WorkletThread();

  void
  RunEventLoop(JSRuntime* aParentRuntime);
  class PrimaryRunnable;

  void
  TerminateInternal();
  class TerminateRunnable;

  // This should only be called by consumers that have an
  // nsIEventTarget/nsIThread pointer.
  NS_IMETHOD
  Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) override;

  NS_IMETHOD
  DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) override;

  NS_IMETHOD
  DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) override;

  const WorkletLoadInfo mWorkletLoadInfo;
  TimeStamp mCreationTimeStamp;

  // Touched only on the worklet thread. This is a raw pointer because it's set
  // and nullified by RunEventLoop().
  JSContext* mJSContext;

  bool mIsTerminating; // main thread
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_worklet_WorkletThread_h
