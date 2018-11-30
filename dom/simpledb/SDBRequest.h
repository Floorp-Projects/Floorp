/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SDBRequest_h
#define mozilla_dom_simpledb_SDBRequest_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISDBRequest.h"
#include "nsIVariant.h"

class nsISDBCallback;

namespace mozilla {
namespace dom {

class SDBConnection;

class SDBRequest final : public nsISDBRequest {
  RefPtr<SDBConnection> mConnection;

  nsCOMPtr<nsIVariant> mResult;
  nsCOMPtr<nsISDBCallback> mCallback;

  nsresult mResultCode;
  bool mHaveResultOrErrorCode;

 public:
  explicit SDBRequest(SDBConnection* aConnection);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(SDBRequest); }

  SDBConnection* GetConnection() const {
    AssertIsOnOwningThread();

    return mConnection;
  }

  void SetResult(nsIVariant* aResult);

  void SetError(nsresult aRv);

 private:
  ~SDBRequest();

  void FireCallback();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSISDBREQUEST
  NS_DECL_CYCLE_COLLECTION_CLASS(SDBRequest)
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_SDBRequest_h
