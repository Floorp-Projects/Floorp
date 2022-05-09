/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageEvent_h
#define mozilla_dom_StorageEvent_h

#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

class nsIPrincipal;

namespace mozilla::dom {

class Storage;
struct StorageEventInit;

class StorageEvent : public Event {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(StorageEvent, Event)

  explicit StorageEvent(EventTarget* aOwner);

 protected:
  virtual ~StorageEvent();

  nsString mKey;
  nsString mOldValue;
  nsString mNewValue;
  nsString mUrl;
  RefPtr<Storage> mStorageArea;
  nsCOMPtr<nsIPrincipal> mPrincipal;

 public:
  virtual StorageEvent* AsStorageEvent();

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<StorageEvent> Constructor(
      EventTarget* aOwner, const nsAString& aType,
      const StorageEventInit& aEventInitDict);

  static already_AddRefed<StorageEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const StorageEventInit& aEventInitDict);

  void InitStorageEvent(const nsAString& aType, bool aCanBubble,
                        bool aCancelable, const nsAString& aKey,
                        const nsAString& aOldValue, const nsAString& aNewValue,
                        const nsAString& aURL, Storage* aStorageArea);

  void GetKey(nsString& aRetVal) const { aRetVal = mKey; }

  void GetOldValue(nsString& aRetVal) const { aRetVal = mOldValue; }

  void GetNewValue(nsString& aRetVal) const { aRetVal = mNewValue; }

  void GetUrl(nsString& aRetVal) const { aRetVal = mUrl; }

  Storage* GetStorageArea() const { return mStorageArea; }

  // Non WebIDL methods
  void SetPrincipal(nsIPrincipal* aPrincipal) {
    MOZ_ASSERT(!mPrincipal);
    mPrincipal = aPrincipal;
  }

  nsIPrincipal* GetPrincipal() const { return mPrincipal; }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_StorageEvent_h
