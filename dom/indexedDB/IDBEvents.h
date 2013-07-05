/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbevents_h__
#define mozilla_dom_indexeddb_idbevents_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBVersionChangeEvent.h"
#include "nsIRunnable.h"

#include "nsDOMEvent.h"
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

class IDBVersionChangeEvent : public nsDOMEvent,
                              public nsIIDBVersionChangeEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIIDBVERSIONCHANGEEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::IDBVersionChangeEventBinding::Wrap(aCx, aScope, this);
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

  inline static already_AddRefed<nsDOMEvent>
  Create(mozilla::dom::EventTarget* aOwner,
         int64_t aOldVersion,
         int64_t aNewVersion)
  {
    return CreateInternal(aOwner,
                          NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsDOMEvent>
  CreateBlocked(mozilla::dom::EventTarget* aOwner,
                uint64_t aOldVersion,
                uint64_t aNewVersion)
  {
    return CreateInternal(aOwner, NS_LITERAL_STRING(BLOCKED_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsDOMEvent>
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
  : nsDOMEvent(aOwner, nullptr, nullptr)
  {
    SetIsDOMBinding();
  }
  virtual ~IDBVersionChangeEvent() { }

  static already_AddRefed<nsDOMEvent>
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
