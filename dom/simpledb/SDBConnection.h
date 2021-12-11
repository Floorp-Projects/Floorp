/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SDBConnection_h
#define mozilla_dom_simpledb_SDBConnection_h

#include <cstdint>
#include "ErrorList.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsISDBConnection.h"
#include "nsISupports.h"

#define NS_SDBCONNECTION_CONTRACTID "@mozilla.org/dom/sdb-connection;1"

class nsISDBCloseCallback;

namespace mozilla {

namespace ipc {

class PBackgroundChild;
class PrincipalInfo;

}  // namespace ipc

namespace dom {

class SDBConnectionChild;
class SDBRequest;
class SDBRequestParams;

class SDBConnection final : public nsISDBConnection {
  using PersistenceType = mozilla::dom::quota::PersistenceType;
  using PBackgroundChild = mozilla::ipc::PBackgroundChild;
  using PrincipalInfo = mozilla::ipc::PrincipalInfo;

  nsCOMPtr<nsISDBCloseCallback> mCloseCallback;

  UniquePtr<PrincipalInfo> mPrincipalInfo;

  SDBConnectionChild* mBackgroundActor;

  PersistenceType mPersistenceType;
  bool mRunningRequest;
  bool mOpen;
  bool mAllowedToClose;

 public:
  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(SDBConnection); }

  void ClearBackgroundActor();

  void OnNewRequest();

  void OnRequestFinished();

  void OnOpen();

  void OnClose(bool aAbnormal);

  void AllowToClose();

 private:
  SDBConnection();

  ~SDBConnection();

  nsresult CheckState();

  nsresult EnsureBackgroundActor();

  nsresult InitiateRequest(SDBRequest* aRequest,
                           const SDBRequestParams& aParams);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISDBCONNECTION
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_simpledb_SDBConnection_h */
