/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbevents_h__
#define mozilla_dom_indexeddb_idbevents_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIRunnable.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"

#define SUCCESS_EVT_STR "success"
#define ERROR_EVT_STR "error"
#define COMPLETE_EVT_STR "complete"
#define ABORT_EVT_STR "abort"
#define VERSIONCHANGE_EVT_STR "versionchange"
#define BLOCKED_EVT_STR "blocked"
#define UPGRADENEEDED_EVT_STR "upgradeneeded"

#define IDBVERSIONCHANGEEVENT_IID \
  { 0x3b65d4c3, 0x73ad, 0x492e, \
    { 0xb1, 0x2d, 0x15, 0xf9, 0xda, 0xc2, 0x08, 0x4b } }

BEGIN_INDEXEDDB_NAMESPACE

enum Bubbles {
  eDoesNotBubble,
  eDoesBubble
};

enum Cancelable {
  eNotCancelable,
  eCancelable
};

already_AddRefed<nsIDOMEvent>
CreateGenericEvent(mozilla::dom::EventTarget* aOwner,
                   const nsAString& aType,
                   Bubbles aBubbles,
                   Cancelable aCancelable);

class IDBVersionChangeEvent : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_EVENT
  NS_DECLARE_STATIC_IID_ACCESSOR(IDBVERSIONCHANGEEVENT_IID)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::IDBVersionChangeEventBinding::Wrap(aCx, aScope, this);
  }

  static already_AddRefed<IDBVersionChangeEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const IDBVersionChangeEventInit& aOptions,
              ErrorResult& aRv)
  {
    uint64_t newVersion = 0;
    if (!aOptions.mNewVersion.IsNull()) {
      newVersion = aOptions.mNewVersion.Value();
    }
    nsCOMPtr<EventTarget> target = do_QueryInterface(aGlobal.GetAsSupports());
    return CreateInternal(target, aType, aOptions.mOldVersion, newVersion);
  }

  uint64_t OldVersion()
  {
    return mOldVersion;
  }

  mozilla::dom::Nullable<uint64_t> GetNewVersion()
  {
    return mNewVersion
      ? mozilla::dom::Nullable<uint64_t>(mNewVersion)
      : mozilla::dom::Nullable<uint64_t>();
  }

  inline static already_AddRefed<Event>
  Create(mozilla::dom::EventTarget* aOwner,
         int64_t aOldVersion,
         int64_t aNewVersion)
  {
    return CreateInternal(aOwner,
                          NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<Event>
  CreateBlocked(mozilla::dom::EventTarget* aOwner,
                uint64_t aOldVersion,
                uint64_t aNewVersion)
  {
    return CreateInternal(aOwner, NS_LITERAL_STRING(BLOCKED_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<Event>
  CreateUpgradeNeeded(mozilla::dom::EventTarget* aOwner,
                      uint64_t aOldVersion,
                      uint64_t aNewVersion)
  {
    return CreateInternal(aOwner,
                          NS_LITERAL_STRING(UPGRADENEEDED_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsIRunnable>
  CreateRunnable(mozilla::dom::EventTarget* aTarget,
                 uint64_t aOldVersion,
                 uint64_t aNewVersion)
  {
    return CreateRunnableInternal(aTarget,
                                  NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                                  aOldVersion, aNewVersion);
  }

  static already_AddRefed<nsIRunnable>
  CreateBlockedRunnable(mozilla::dom::EventTarget* aTarget,
                        uint64_t aOldVersion,
                        uint64_t aNewVersion)
  {
    return CreateRunnableInternal(aTarget,
                                  NS_LITERAL_STRING(BLOCKED_EVT_STR),
                                  aOldVersion, aNewVersion);
  }

protected:
  IDBVersionChangeEvent(mozilla::dom::EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr)
  {
    SetIsDOMBinding();
  }
  virtual ~IDBVersionChangeEvent() { }

  static already_AddRefed<IDBVersionChangeEvent>
  CreateInternal(mozilla::dom::EventTarget* aOwner,
                 const nsAString& aType,
                 uint64_t aOldVersion,
                 uint64_t aNewVersion);

  static already_AddRefed<nsIRunnable>
  CreateRunnableInternal(mozilla::dom::EventTarget* aOwner,
                         const nsAString& aType,
                         uint64_t aOldVersion,
                         uint64_t aNewVersion);

  uint64_t mOldVersion;
  uint64_t mNewVersion;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbevents_h__
