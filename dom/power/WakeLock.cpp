/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WakeLock.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Event.h"  // for Event
#include "mozilla/Hal.h"
#include "mozilla/HalWakeLock.h"
#include "nsError.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsIPropertyBag2.h"

using namespace mozilla::hal;

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN(WakeLock)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIWakeLock)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WakeLock)
NS_IMPL_RELEASE(WakeLock)

WakeLock::~WakeLock() {
  DoUnlock();
  DetachEventListener();
}

nsresult WakeLock::Init(const nsAString& aTopic, nsPIDOMWindowInner* aWindow) {
  // Don't Init() a WakeLock twice.
  MOZ_ASSERT(mTopic.IsEmpty());

  if (aTopic.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mTopic.Assign(aTopic);

  mWindow = do_GetWeakReference(aWindow);

  /**
   * Null windows are allowed. A wake lock without associated window
   * is always considered invisible.
   */
  if (aWindow) {
    nsCOMPtr<Document> doc = aWindow->GetExtantDoc();
    NS_ENSURE_STATE(doc);
    mHidden = doc->Hidden();
  }

  AttachEventListener();
  DoLock();

  return NS_OK;
}

void WakeLock::DoLock() {
  if (!mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = true;

    hal::ModifyWakeLock(
        mTopic, hal::WAKE_LOCK_ADD_ONE,
        mHidden ? hal::WAKE_LOCK_ADD_ONE : hal::WAKE_LOCK_NO_CHANGE);
  }
}

void WakeLock::DoUnlock() {
  if (mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = false;

    hal::ModifyWakeLock(
        mTopic, hal::WAKE_LOCK_REMOVE_ONE,
        mHidden ? hal::WAKE_LOCK_REMOVE_ONE : hal::WAKE_LOCK_NO_CHANGE);
  }
}

void WakeLock::AttachEventListener() {
  if (nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow)) {
    nsCOMPtr<Document> doc = window->GetExtantDoc();
    if (doc) {
      doc->AddSystemEventListener(u"visibilitychange"_ns, this,
                                  /* useCapture = */ true,
                                  /* wantsUntrusted = */ false);

      nsCOMPtr<EventTarget> target = do_QueryInterface(window);
      target->AddSystemEventListener(u"pagehide"_ns, this,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);
      target->AddSystemEventListener(u"pageshow"_ns, this,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);
    }
  }
}

void WakeLock::DetachEventListener() {
  if (nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow)) {
    nsCOMPtr<Document> doc = window->GetExtantDoc();
    if (doc) {
      doc->RemoveSystemEventListener(u"visibilitychange"_ns, this,
                                     /* useCapture = */ true);
      nsCOMPtr<EventTarget> target = do_QueryInterface(window);
      target->RemoveSystemEventListener(u"pagehide"_ns, this,
                                        /* useCapture = */ true);
      target->RemoveSystemEventListener(u"pageshow"_ns, this,
                                        /* useCapture = */ true);
    }
  }
}

void WakeLock::Unlock(ErrorResult& aRv) {
  /*
   * We throw NS_ERROR_DOM_INVALID_STATE_ERR on double unlock.
   */
  if (!mLocked) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  DoUnlock();
  DetachEventListener();
}

void WakeLock::GetTopic(nsAString& aTopic) { aTopic.Assign(mTopic); }

NS_IMETHODIMP
WakeLock::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("visibilitychange")) {
    nsCOMPtr<Document> doc = do_QueryInterface(aEvent->GetTarget());
    NS_ENSURE_STATE(doc);

    bool oldHidden = mHidden;
    mHidden = doc->Hidden();

    if (mLocked && oldHidden != mHidden) {
      hal::ModifyWakeLock(
          mTopic, hal::WAKE_LOCK_NO_CHANGE,
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

NS_IMETHODIMP
WakeLock::Unlock() {
  ErrorResult error;
  Unlock(error);
  return error.StealNSResult();
}

nsPIDOMWindowInner* WakeLock::GetParentObject() const {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mWindow);
  return window;
}

}  // namespace mozilla::dom
