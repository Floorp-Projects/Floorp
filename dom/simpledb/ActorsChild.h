/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_ActorsChild_h
#define mozilla_dom_simpledb_ActorsChild_h

#include <cstdint>
#include "ErrorList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/PBackgroundSDBConnectionChild.h"
#include "mozilla/dom/PBackgroundSDBRequestChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace ipc {

class BackgroundChildImpl;

}  // namespace ipc

namespace dom {

class SDBConnection;
class SDBRequest;

class SDBConnectionChild final : public PBackgroundSDBConnectionChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class SDBConnection;

  SDBConnection* mConnection;

  NS_INLINE_DECL_REFCOUNTING(SDBConnectionChild, override)

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(SDBConnectionChild);
  }

 private:
  // Only created by SDBConnection.
  explicit SDBConnectionChild(SDBConnection* aConnection);

  ~SDBConnectionChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBackgroundSDBRequestChild* AllocPBackgroundSDBRequestChild(
      const SDBRequestParams& aParams) override;

  virtual bool DeallocPBackgroundSDBRequestChild(
      PBackgroundSDBRequestChild* aActor) override;

  virtual mozilla::ipc::IPCResult RecvAllowToClose() override;

  virtual mozilla::ipc::IPCResult RecvClosed() override;
};

class SDBRequestChild final : public PBackgroundSDBRequestChild {
  friend class SDBConnectionChild;
  friend class SDBConnection;

  RefPtr<SDBConnection> mConnection;
  RefPtr<SDBRequest> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 private:
  // Only created by SDBConnection.
  explicit SDBRequestChild(SDBRequest* aRequest);

  // Only destroyed by SDBConnectionChild.
  ~SDBRequestChild();

  void HandleResponse(nsresult aResponse);

  void HandleResponse();

  void HandleResponse(const nsCString& aResponse);

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult Recv__delete__(
      const SDBRequestResponse& aResponse) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_ActorsChild_h
