/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_GLOBALTEARDOWNOBSERVER_H_
#define DOM_BASE_GLOBALTEARDOWNOBSERVER_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsPIDOMWindow.h"

#define NS_GLOBALTEARDOWNOBSERVER_IID                \
  {                                                  \
    0xc31fddb9, 0xec49, 0x4f24, {                    \
      0x90, 0x16, 0xb5, 0x2b, 0x26, 0x6c, 0xb6, 0x29 \
    }                                                \
  }

namespace mozilla {

class GlobalTeardownObserver
    : public nsISupports,
      public LinkedListElement<GlobalTeardownObserver> {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_GLOBALTEARDOWNOBSERVER_IID)

  GlobalTeardownObserver();
  explicit GlobalTeardownObserver(nsIGlobalObject* aGlobalObject,
                                  bool aHasOrHasHadOwnerWindow = false);

  nsPIDOMWindowInner* GetOwner() const { return mOwnerWindow; }
  nsIGlobalObject* GetOwnerGlobal() const { return mParentObject; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwnerWindow; }

  void GetParentObject(nsIScriptGlobalObject** aParentObject) {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  virtual void DisconnectFromOwner();

  // A global permanently becomes invalid when DisconnectEventTargetObjects() is
  // called.  Normally this means:
  // - For the main thread, when nsGlobalWindowInner::FreeInnerObjects is
  //   called.
  // - For a worker thread, when clearing the main event queue.  (Which we do
  //   slightly later than when the spec notionally calls for it to be done.)
  //
  // A global may also become temporarily invalid when:
  // - For the main thread, if the window is no longer the WindowProxy's current
  //   inner window due to being placed in the bfcache.
  nsresult CheckCurrentGlobalCorrectness() const;

 protected:
  virtual ~GlobalTeardownObserver();

  void BindToOwner(nsIGlobalObject* aOwner);

 private:
  // The parent global object.  The global will clear this when
  // it is destroyed by calling DisconnectFromOwner().
  nsIGlobalObject* MOZ_NON_OWNING_REF mParentObject = nullptr;
  // mParentObject pre QI-ed and cached (inner window)
  // (it is needed for off main thread access)
  // It is obtained in BindToOwner and reset in DisconnectFromOwner.
  nsPIDOMWindowInner* MOZ_NON_OWNING_REF mOwnerWindow = nullptr;
  bool mHasOrHasHadOwnerWindow = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(GlobalTeardownObserver,
                              NS_GLOBALTEARDOWNOBSERVER_IID)

}  // namespace mozilla

#endif
