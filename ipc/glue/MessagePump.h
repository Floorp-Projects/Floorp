/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __IPC_GLUE_MESSAGEPUMP_H__
#define __IPC_GLUE_MESSAGEPUMP_H__

#include "base/basictypes.h"
#include "base/message_pump_default.h"
#include "base/time.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsITimer.h"

namespace mozilla {
namespace ipc {

class MessagePump;

class DoWorkRunnable : public nsIRunnable,
                       public nsITimerCallback
{
public:
  DoWorkRunnable(MessagePump* aPump)
  : mPump(aPump) { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITIMERCALLBACK

private:
  MessagePump* mPump;
};

class MessagePump : public base::MessagePumpDefault
{

public:
  MessagePump();

  virtual void Run(base::MessagePump::Delegate* aDelegate);
  virtual void ScheduleWork();
  virtual void ScheduleWorkForNestedLoop();
  virtual void ScheduleDelayedWork(const base::Time& delayed_work_time);

  void DoDelayedWork(base::MessagePump::Delegate* aDelegate);

private:
  nsRefPtr<DoWorkRunnable> mDoWorkEvent;
  nsCOMPtr<nsITimer> mDelayedWorkTimer;

  // Weak!
  nsIThread* mThread;
};

class MessagePumpForChildProcess : public MessagePump
{
public:
  MessagePumpForChildProcess()
  : mFirstRun(true)
  { }

  virtual void Run(base::MessagePump::Delegate* aDelegate);

private:
  bool mFirstRun;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_MESSAGEPUMP_H__ */
