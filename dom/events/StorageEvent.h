/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageEvent_h
#define mozilla_dom_StorageEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/StorageEventBinding.h"

class nsIDOMStorage;

// Helper for EventDispatcher.
nsresult NS_NewDOMStorageEvent(nsIDOMEvent** aDOMEvent,
                               mozilla::dom::EventTarget* aOwner);

namespace mozilla {
namespace dom {

class StorageEvent : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(StorageEvent, Event)

  StorageEvent(EventTarget* aOwner);
  virtual ~StorageEvent();

protected:
  nsString mKey;
  nsString mOldValue;
  nsString mNewValue;
  nsString mUrl;
  nsCOMPtr<nsIDOMStorage> mStorageArea;

public:
  virtual StorageEvent* AsStorageEvent();

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<StorageEvent>
  Constructor(EventTarget* aOwner, const nsAString& aType,
              const StorageEventInit& aEventInitDict);

  static already_AddRefed<StorageEvent>
  Constructor(const GlobalObject& aGlobal, const nsAString& aType,
              const StorageEventInit& aEventInitDict, ErrorResult& aRv);

  void InitStorageEvent(const nsAString& aType, bool aCanBubble,
                        bool aCancelable, const nsAString& aKey,
                        const nsAString& aOldValue,
                        const nsAString& aNewValue,
                        const nsAString& aURL,
                        nsIDOMStorage* aStorageArea,
                        ErrorResult& aRv);

  void GetKey(nsString& aRetVal) const
  {
    aRetVal = mKey;
  }

  void GetOldValue(nsString& aRetVal) const
  {
    aRetVal = mOldValue;
  }

  void GetNewValue(nsString& aRetVal) const
  {
    aRetVal = mNewValue;
  }

  void GetUrl(nsString& aRetVal) const
  {
    aRetVal = mUrl;
  }

  nsIDOMStorage* GetStorageArea() const
  {
    return mStorageArea;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageEvent_h
