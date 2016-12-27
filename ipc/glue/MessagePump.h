/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_MESSAGEPUMP_H__
#define __IPC_GLUE_MESSAGEPUMP_H__

#include "base/message_pump_default.h"
#if defined(XP_WIN)
#include "base/message_pump_win.h"
#endif

#include "base/time.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIThreadInternal.h"

class nsIThread;
class nsITimer;

namespace mozilla {
namespace ipc {

class DoWorkRunnable;

class MessagePump : public base::MessagePumpDefault
{
  friend class DoWorkRunnable;

public:
  explicit MessagePump(nsIThread* aThread);

  // From base::MessagePump.
  virtual void
  Run(base::MessagePump::Delegate* aDelegate) override;

  // From base::MessagePump.
  virtual void
  ScheduleWork() override;

  // From base::MessagePump.
  virtual void
  ScheduleWorkForNestedLoop() override;

  // From base::MessagePump.
  virtual void
  ScheduleDelayedWork(const base::TimeTicks& aDelayedWorkTime) override;

  virtual nsIEventTarget*
  GetXPCOMThread() override;

protected:
  virtual ~MessagePump();

private:
  // Only called by DoWorkRunnable.
  void DoDelayedWork(base::MessagePump::Delegate* aDelegate);

protected:
  nsIThread* mThread;

  // mDelayedWorkTimer and mThread are set in Run() by this class or its
  // subclasses.
  nsCOMPtr<nsITimer> mDelayedWorkTimer;

private:
  // Only accessed by this class.
  RefPtr<DoWorkRunnable> mDoWorkEvent;
};

class MessagePumpForChildProcess final: public MessagePump
{
public:
  MessagePumpForChildProcess()
    : MessagePump(nullptr),
      mFirstRun(true)
  { }

  virtual void Run(base::MessagePump::Delegate* aDelegate) override;

private:
  ~MessagePumpForChildProcess()
  { }

  bool mFirstRun;
};

class MessagePumpForNonMainThreads final : public MessagePump
{
public:
  explicit MessagePumpForNonMainThreads(nsIThread* aThread)
    : MessagePump(aThread)
  { }

  virtual void Run(base::MessagePump::Delegate* aDelegate) override;

private:
  ~MessagePumpForNonMainThreads()
  { }
};

#if defined(XP_WIN)
// Extends the TYPE_UI message pump to process xpcom events. Currently only
// implemented for Win.
class MessagePumpForNonMainUIThreads final:
  public base::MessagePumpForUI,
  public nsIThreadObserver
{
public:
  // We don't want xpcom refing, chromium controls our lifetime via
  // RefCountedThreadSafe.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override {
    return 2;
  }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override  {
    return 1;
  }
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_NSITHREADOBSERVER

public:
  explicit MessagePumpForNonMainUIThreads(nsIThread* aThread) :
    mInWait(false),
    mWaitLock("mInWait")
  {
  }

  // The main run loop for this thread.
  virtual void DoRunLoop() override;

  virtual nsIEventTarget*
  GetXPCOMThread() override
  {
    return nullptr; // not sure what to do with this one
  }

protected:
  void SetInWait() {
    MutexAutoLock lock(mWaitLock);
    mInWait = true;
  }

  void ClearInWait() {
    MutexAutoLock lock(mWaitLock);
    mInWait = false;
  }

  bool GetInWait() {
    MutexAutoLock lock(mWaitLock);
    return mInWait;
  }

private:
  ~MessagePumpForNonMainUIThreads()
  {
  }

  bool mInWait;
  mozilla::Mutex mWaitLock;
};
#endif // defined(XP_WIN)

#if defined(MOZ_WIDGET_ANDROID)
/*`
 * The MessagePumpForAndroidUI exists to enable IPDL in the Android UI thread. The Android
 * UI thread event loop is controlled by Android. This prevents running an existing
 * MessagePump implementation in the Android UI thread. In order to enable IPDL on the
 * Android UI thread it is necessary to have a non-looping MessagePump. This class enables
 * forwarding of nsIRunnables from MessageLoop::PostTask_Helper to the registered
 * nsIEventTarget with out the need to control the event loop. The only member function
 * that should be invoked is GetXPCOMThread. All other member functions will invoke MOZ_CRASH
*/
class MessagePumpForAndroidUI : public base::MessagePump {

public:
  MessagePumpForAndroidUI(nsIEventTarget* aEventTarget)
    : mEventTarget(aEventTarget)
  { }

  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const base::TimeTicks& delayed_work_time);
  virtual nsIEventTarget* GetXPCOMThread()
  {
    return mEventTarget;
  }

private:
  ~MessagePumpForAndroidUI()
  { }
  MessagePumpForAndroidUI()
  { }

  nsIEventTarget* mEventTarget;
};
#endif // defined(MOZ_WIDGET_ANDROID)


} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_MESSAGEPUMP_H__ */
