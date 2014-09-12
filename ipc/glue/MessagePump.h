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
#include "nsAutoPtr.h"
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
  MessagePump();

  // From base::MessagePump.
  virtual void
  Run(base::MessagePump::Delegate* aDelegate) MOZ_OVERRIDE;

  // From base::MessagePump.
  virtual void
  ScheduleWork() MOZ_OVERRIDE;

  // From base::MessagePump.
  virtual void
  ScheduleWorkForNestedLoop() MOZ_OVERRIDE;

  // From base::MessagePump.
  virtual void
  ScheduleDelayedWork(const base::TimeTicks& aDelayedWorkTime) MOZ_OVERRIDE;

protected:
  virtual ~MessagePump();

private:
  // Only called by DoWorkRunnable.
  void DoDelayedWork(base::MessagePump::Delegate* aDelegate);

protected:
  // mDelayedWorkTimer and mThread are set in Run() by this class or its
  // subclasses.
  nsCOMPtr<nsITimer> mDelayedWorkTimer;
  nsIThread* mThread;

private:
  // Only accessed by this class.
  nsRefPtr<DoWorkRunnable> mDoWorkEvent;
};

class MessagePumpForChildProcess MOZ_FINAL: public MessagePump
{
public:
  MessagePumpForChildProcess()
  : mFirstRun(true)
  { }

  virtual void Run(base::MessagePump::Delegate* aDelegate) MOZ_OVERRIDE;

private:
  ~MessagePumpForChildProcess()
  { }

  bool mFirstRun;
};

class MessagePumpForNonMainThreads MOZ_FINAL : public MessagePump
{
public:
  MessagePumpForNonMainThreads()
  { }

  virtual void Run(base::MessagePump::Delegate* aDelegate) MOZ_OVERRIDE;

private:
  ~MessagePumpForNonMainThreads()
  { }
};

#if defined(XP_WIN)
// Extends the TYPE_UI message pump to process xpcom events. Currently only
// implemented for Win.
class MessagePumpForNonMainUIThreads MOZ_FINAL:
  public base::MessagePumpForUI,
  public nsIThreadObserver
{
public:
  // We don't want xpcom refing, chromium controls our lifetime via
  // RefCountedThreadSafe.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) {
    return 2;
  }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) {
    return 1;
  }
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_NSITHREADOBSERVER

public:
  MessagePumpForNonMainUIThreads() :
    mThread(nullptr),
    mInWait(false),
    mWaitLock("mInWait")
  {
  }

  // The main run loop for this thread.
  virtual void DoRunLoop();

protected:
  nsIThread* mThread;

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

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_MESSAGEPUMP_H__ */
