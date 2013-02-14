/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_power_WakeLock_h
#define mozilla_dom_power_WakeLock_h

#include "nsCOMPtr.h"
#include "nsIDOMWakeLock.h"
#include "nsIDOMEventListener.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIDOMWindow;

namespace mozilla {
namespace dom {
namespace power {

class WakeLock
  : public nsIDOMMozWakeLock
  , public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZWAKELOCK
  NS_DECL_NSIDOMEVENTLISTENER

  WakeLock();
  virtual ~WakeLock();

  nsresult Init(const nsAString &aTopic, nsIDOMWindow *aWindow);

private:
  void     DoUnlock();
  void     DoLock();
  void     AttachEventListener();
  void     DetachEventListener();

  bool      mLocked;
  bool      mHidden;
  nsString  mTopic;

  // window that this was created for.  Weak reference.
  nsWeakPtr mWindow;
};

} // namespace power
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_WakeLock_h
