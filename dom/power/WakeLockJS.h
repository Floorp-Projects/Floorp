/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WAKELOCKJS_H_
#define DOM_WAKELOCKJS_H_

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/HalBatteryInformation.h"
#include "mozilla/dom/WakeLockBinding.h"
#include "nsIDOMEventListener.h"
#include "nsIDocumentActivity.h"
#include "nsIObserver.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

class Promise;
class Document;
class WakeLockSentinel;

}  // namespace mozilla::dom

namespace mozilla::dom {

/**
 * Management class for wake locks held from client scripts.
 * Instances of this class have two purposes:
 * - Implement navigator.wakeLock.request which creates a WakeLockSentinel
 * - Listen for state changes that require all WakeLockSentinel to be released
 * The WakeLockSentinel objects are held in document.mActiveLocks.
 *
 * https://www.w3.org/TR/screen-wake-lock/#the-wakelock-interface
 */
class WakeLockJS final : public nsIDOMEventListener,
                         public nsWrapperCache,
                         public hal::BatteryObserver,
                         public nsIDocumentActivity,
                         public nsIObserver,
                         public nsSupportsWeakReference {
 public:
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIDOCUMENTACTIVITY
  NS_DECL_NSIOBSERVER

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(WakeLockJS,
                                                        nsIDOMEventListener)

 public:
  explicit WakeLockJS(nsPIDOMWindowInner* aWindow);

 protected:
  ~WakeLockJS();

 public:
  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void Notify(const hal::BatteryInformation& aBatteryInfo) override;

  already_AddRefed<Promise> Request(WakeLockType aType, ErrorResult& aRv);

 private:
  enum class RequestError {
    Success,
    DocInactive,
    DocHidden,
    PolicyDisallowed,
    PrefDisabled,
    InternalFailure,
    PermissionDenied
  };

  static nsLiteralCString GetRequestErrorMessage(RequestError aRv);

  static RequestError WakeLockAllowedForDocument(Document* aDoc);

  void AttachListeners();
  void DetachListeners();

  Result<already_AddRefed<WakeLockSentinel>, RequestError> Obtain(
      WakeLockType aType);

  RefPtr<nsPIDOMWindowInner> mWindow;
};

MOZ_CAN_RUN_SCRIPT
void ReleaseWakeLock(Document* aDoc, WakeLockSentinel* aLock,
                     WakeLockType aType);

}  // namespace mozilla::dom

#endif  // DOM_WAKELOCKJS_H_
