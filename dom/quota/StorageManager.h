/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageManager_h
#define mozilla_dom_StorageManager_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Promise;
struct StorageEstimate;

class StorageManager final
  : public nsISupports
  , public nsWrapperCache
{
  nsCOMPtr<nsIGlobalObject> mOwner;

public:
  explicit
  StorageManager(nsIGlobalObject* aGlobal);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mOwner;
  }

  // WebIDL
  already_AddRefed<Promise>
  Persisted(ErrorResult& aRv);

  already_AddRefed<Promise>
  Persist(ErrorResult& aRv);

  already_AddRefed<Promise>
  Estimate(ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StorageManager)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~StorageManager();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageManager_h
