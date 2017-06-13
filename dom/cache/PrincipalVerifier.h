/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_PrincipalVerifier_h
#define mozilla_dom_cache_PrincipalVerifier_h

#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsThreadUtils.h"
#include "nsTObserverArray.h"

namespace mozilla {

namespace ipc {
  class PBackgroundParent;
} // namespace ipc

namespace dom {
namespace cache {

class ManagerId;

class PrincipalVerifier final : public Runnable
{
public:
  // An interface to be implemented by code wishing to use the
  // PrincipalVerifier.  Note, the Listener implementation is responsible
  // for calling RemoveListener() on the PrincipalVerifier to clear the
  // weak reference.
  class Listener
  {
  public:
    virtual void OnPrincipalVerified(nsresult aRv, ManagerId* aManagerId) = 0;
  };

  static already_AddRefed<PrincipalVerifier>
  CreateAndDispatch(Listener* aListener, mozilla::ipc::PBackgroundParent* aActor,
                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

  void AddListener(Listener* aListener);

  // The Listener must call RemoveListener() when OnPrincipalVerified() is
  // called or when the Listener is destroyed.
  void RemoveListener(Listener* aListener);

private:
  PrincipalVerifier(Listener* aListener, mozilla::ipc::PBackgroundParent* aActor,
                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);
  virtual ~PrincipalVerifier();

  void VerifyOnMainThread();
  void CompleteOnInitiatingThread();

  void DispatchToInitiatingThread(nsresult aRv);

  // Weak reference cleared by RemoveListener()
  typedef nsTObserverArray<Listener*> ListenerList;
  ListenerList mListenerList;

  // set in originating thread at construction, but must be accessed and
  // released on main thread
  RefPtr<ContentParent> mActor;

  const mozilla::ipc::PrincipalInfo mPrincipalInfo;
  nsCOMPtr<nsIEventTarget> mInitiatingEventTarget;
  nsresult mResult;
  RefPtr<ManagerId> mManagerId;

public:
  NS_DECL_NSIRUNNABLE
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_PrincipalVerifier_h
