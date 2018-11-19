/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedWorkerManager_h
#define mozilla_dom_SharedWorkerManager_h

#include "nsISupportsImpl.h"
#include "nsTArray.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class MessagePortIdentifier;
class RemoteWorkerData;
class RemoteWorkerController;
class SharedWorkerParent;

class SharedWorkerManager final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerManager);

  // Called on main-thread thread methods

  SharedWorkerManager(nsIEventTarget* aPBackgroundEventTarget,
                      const RemoteWorkerData& aData,
                      nsIPrincipal* aLoadingPrincipal);

  bool
  MatchOnMainThread(const nsACString& aDomain,
                    const nsACString& aScriptURL,
                    const nsAString& aName,
                    nsIPrincipal* aLoadingPrincipal) const;

  // Called on PBackground thread methods

  bool
  MaybeCreateRemoteWorker(const RemoteWorkerData& aData,
                          uint64_t aWindowID,
                          const MessagePortIdentifier& aPortIdentifier);

  void
  AddActor(SharedWorkerParent* aParent);

  void
  RemoveActor(SharedWorkerParent* aParent);

  void
  UpdateSuspend();

  void
  UpdateFrozen();

  bool
  IsSecureContext() const;

private:
  ~SharedWorkerManager();

  nsCOMPtr<nsIEventTarget> mPBackgroundEventTarget;

  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCString mDomain;
  nsCString mResolvedScriptURL;
  nsString mName;
  bool mIsSecureContext;

  // Raw pointers because SharedWorkerParent unregisters itself in ActorDestroy().
  nsTArray<SharedWorkerParent*> mActors;

  RefPtr<RemoteWorkerController> mRemoteWorkerController;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_SharedWorkerManager_h
