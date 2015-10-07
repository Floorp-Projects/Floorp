/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataStoreRevision_h
#define mozilla_dom_DataStoreRevision_h

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventListener.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

namespace indexedDB {
class IDBObjectStore;
class IDBRequest;
} // namespace indexedDB

class DataStoreRevisionCallback;

class DataStoreRevision final : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  enum RevisionType {
    RevisionVoid
  };

  nsresult AddRevision(JSContext* aCx,
                       indexedDB::IDBObjectStore* aStore,
                       uint32_t aObjectId,
                       RevisionType aRevisionType,
                       DataStoreRevisionCallback* aCallback);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

private:
  ~DataStoreRevision() {}
  nsRefPtr<DataStoreRevisionCallback> mCallback;
  nsRefPtr<indexedDB::IDBRequest> mRequest;
  nsString mRevisionID;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DataStoreRevision_h
