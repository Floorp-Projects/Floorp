/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_STORAGE_SESSIONSTORAGESERVICE_H_
#define DOM_STORAGE_SESSIONSTORAGESERVICE_H_

#include "nsISessionStorageService.h"
#include "mozilla/Result.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PBackgroundSessionStorageServiceChild.h"

namespace mozilla {

struct CreateIfNonExistent;

namespace dom {

class SessionStorageService final
    : public nsISessionStorageService,
      public PBackgroundSessionStorageServiceChild {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISESSIONSTORAGESERVICE

  SessionStorageService();

  // Singleton Boilerplate
  static mozilla::Result<RefPtr<SessionStorageService>, nsresult> Acquire(
      const CreateIfNonExistent&);

  // Can return null if the service hasn't be created yet or after
  // XPCOMShutdown.
  static RefPtr<SessionStorageService> Acquire();

 private:
  ~SessionStorageService();

  mozilla::Result<Ok, nsresult> Init();

  void Shutdown();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  FlippedOnce<false> mInitialized;
  FlippedOnce<false> mActorDestroyed;
};

}  // namespace dom
}  // namespace mozilla

#endif /* DOM_STORAGE_SESSIONSTORAGESERVICE_H_ */
