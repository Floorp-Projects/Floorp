/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_MESSAGEPUMP_H__
#define __IPC_GLUE_MESSAGEPUMP_H__

#include "base/message_pump_default.h"
#include "base/time.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

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

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_MESSAGEPUMP_H__ */
