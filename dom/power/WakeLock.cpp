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
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIWakeLock)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WakeLock)
NS_IMPL_RELEASE(WakeLock)

WakeLock::WakeLock()
    : mLocked(false),
      mHidden(true),
      mContentParentID(CONTENT_PROCESS_ID_UNKNOWN) {}

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
    mHidden = IsDocumentInvisible(*doc);
  }

  AttachEventListener();
  DoLock();

  return NS_OK;
}

NS_IMETHODIMP
WakeLock::Observe(nsISupports* aSubject, const char* aTopic,
                  const char16_t* data) {
  // If this wake lock was acquired on behalf of another process, unlock it
  // when that process dies.
  //
  // Note that we do /not/ call DoUnlock() here!  The wake lock back-end is
  // already listening for ipc:content-shutdown messages and will clear out its
  // tally for the process when it dies.  All we need to do here is ensure that
  // unlock() becomes a nop.

  MOZ_ASSERT(!strcmp(aTopic, "ipc:content-shutdown"));

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  if (!props) {
    NS_WARNING("ipc:content-shutdown message without property bag as subject");
    return NS_OK;
  }

  uint64_t childID = 0;
  nsresult rv = props->GetPropertyAsUint64(u"childID"_ns, &childID);
  if (NS_SUCCEEDED(rv)) {
    if (childID == mContentParentID) {
      mLocked = false;
    }
  } else {
    NS_WARNING("ipc:content-shutdown message without childID property");
  }
  return NS_OK;
}

void WakeLock::DoLock() {
  if (!mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = true;

    hal::ModifyWakeLock(
        mTopic, hal::WAKE_LOCK_ADD_ONE,
        mHidden ? hal::WAKE_LOCK_ADD_ONE : hal::WAKE_LOCK_NO_CHANGE,
        mContentParentID);
  }
}

void WakeLock::DoUnlock() {
  if (mLocked) {
    // Change the flag immediately to prevent recursive reentering
    mLocked = false;

    hal::ModifyWakeLock(
        mTopic, hal::WAKE_LOCK_REMOVE_ONE,
        mHidden ? hal::WAKE_LOCK_REMOVE_ONE : hal::WAKE_LOCK_NO_CHANGE,
        mContentParentID);
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

bool WakeLock::IsDocumentInvisible(const Document& aDocument) const {
  // If document has a child element being used in the picture in picture
  // mode, which is always visible to users, then we would consider the
  // document as visible as well.
  return aDocument.Hidden() && !aDocument.HasPictureInPictureChildElement();
}

NS_IMETHODIMP
WakeLock::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("visibilitychange")) {
    nsCOMPtr<Document> doc = do_QueryInterface(aEvent->GetTarget());
    NS_ENSURE_STATE(doc);

    bool oldHidden = mHidden;
    mHidden = IsDocumentInvisible(*doc);

    if (mLocked && oldHidden != mHidden) {
      hal::ModifyWakeLock(
          mTopic, hal::WAKE_LOCK_NO_CHANGE,
          mHidden ? hal::WAKE_LOCK_ADD_ONE : hal::WAKE_LOCK_REMOVE_ONE,
          mContentParentID);
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
