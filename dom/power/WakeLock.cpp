/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalWakeLock.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMError.h"
#include "nsIDOMWindow.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "PowerManager.h"
#include "WakeLock.h"

DOMCI_DATA(MozWakeLock, mozilla::dom::power::WakeLock)

namespace mozilla {
namespace dom {
namespace power {

NS_INTERFACE_MAP_BEGIN(WakeLock)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozWakeLock)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozWakeLock)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozWakeLock)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WakeLock)
NS_IMPL_RELEASE(WakeLock)

WakeLock::WakeLock()
  : mLocked(false)
  , mHidden(true)
{
}

WakeLock::~WakeLock()
{
  DoUnlock();
  DetachEventListener();
}

nsresult
WakeLock::Init(const nsAString &aTopic, nsIDOMWindow *aWindow)
{
  mTopic.Assign(aTopic);

  mWindow = do_GetWeakReference(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);

  /**
   * Null windows are allowed. A wake lock without associated window
   * is always considered invisible.
   */
  if (window) {
    nsCOMPtr<nsIDOMDocument> domDoc = window->GetExtantDocument();
    NS_ENSURE_STATE(domDoc);
    domDoc->GetMozHidden(&mHidden);
  }

  AttachEventListener();
  DoLock();

  return NS_OK;
}

void
WakeLock::DoLock()
{
  if (!mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = true;
    hal::ModifyWakeLock(mTopic,
                        hal::WAKE_LOCK_ADD_ONE,
                        mHidden ? hal::WAKE_LOCK_ADD_ONE : hal::WAKE_LOCK_NO_CHANGE);
  }
}

void
WakeLock::DoUnlock()
{
  if (mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = false;
    hal::ModifyWakeLock(mTopic,
                        hal::WAKE_LOCK_REMOVE_ONE,
                        mHidden ? hal::WAKE_LOCK_REMOVE_ONE : hal::WAKE_LOCK_NO_CHANGE);
  }
}

void
WakeLock::AttachEventListener()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  
  if (window) {
    nsCOMPtr<nsIDOMDocument> domDoc = window->GetExtantDocument();
    if (domDoc) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(domDoc);
      target->AddSystemEventListener(NS_LITERAL_STRING("mozvisibilitychange"),
                                     this,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);

      target = do_QueryInterface(window);
      target->AddSystemEventListener(NS_LITERAL_STRING("pagehide"),
                                     this,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);
      target->AddSystemEventListener(NS_LITERAL_STRING("pageshow"),
                                     this,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);
    }
  }
}

void
WakeLock::DetachEventListener()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);

  if (window) {
    nsCOMPtr<nsIDOMDocument> domDoc = window->GetExtantDocument();
    if (domDoc) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(domDoc);
      target->RemoveSystemEventListener(NS_LITERAL_STRING("mozvisibilitychange"),
                                        this,
                                        /* useCapture = */ true);
      target = do_QueryInterface(window);
      target->RemoveSystemEventListener(NS_LITERAL_STRING("pagehide"),
                                        this,
                                        /* useCapture = */ true);
      target->RemoveSystemEventListener(NS_LITERAL_STRING("pageshow"),
                                        this,
                                        /* useCapture = */ true);
    }
  }
}

NS_IMETHODIMP
WakeLock::Unlock()
{
  /*
   * We throw NS_ERROR_DOM_INVALID_STATE_ERR on double unlock.
   */
  if (!mLocked) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  DoUnlock();
  DetachEventListener();

  return NS_OK;
}

NS_IMETHODIMP
WakeLock::GetTopic(nsAString &aTopic)
{
  aTopic.Assign(mTopic);
  return NS_OK;
}

NS_IMETHODIMP
WakeLock::HandleEvent(nsIDOMEvent *aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("mozvisibilitychange")) {
    nsCOMPtr<nsIDOMEventTarget> target;
    aEvent->GetTarget(getter_AddRefs(target));
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(target);
    NS_ENSURE_STATE(domDoc);
    domDoc->GetMozHidden(&mHidden);

    if (mLocked) {
      hal::ModifyWakeLock(mTopic,
                          hal::WAKE_LOCK_NO_CHANGE,
                          mHidden ? hal::WAKE_LOCK_ADD_ONE : hal::WAKE_LOCK_REMOVE_ONE);
    }

    return NS_OK;
  }

  if (type.EqualsLiteral("pagehide")) {
    DoUnlock();
    return NS_OK;
  }

  if (type.EqualsLiteral("pageshow")) {
    DoLock();
    return NS_OK;
  }

  return NS_OK;
}

} // power
} // dom
} // mozilla
