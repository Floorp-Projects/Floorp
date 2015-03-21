/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_PrincipalVerifier_h
#define mozilla_dom_cache_PrincipalVerifier_h

#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsThreadUtils.h"

namespace mozilla {

namespace ipc {
  class PBackgroundParent;
}

namespace dom {
namespace cache {

class ManagerId;

class PrincipalVerifier final : public nsRunnable
{
public:
  // An interface to be implemented by code wishing to use the
  // PrincipalVerifier.  Note, the Listener implementation is responsible
  // for calling ClearListener() on the PrincipalVerifier to clear the
  // weak reference.
  class Listener
  {
  public:
    virtual void OnPrincipalVerified(nsresult aRv, ManagerId* aManagerId) = 0;
  };

  static already_AddRefed<PrincipalVerifier>
  CreateAndDispatch(Listener* aListener, mozilla::ipc::PBackgroundParent* aActor,
                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

  // The Listener must call ClearListener() when OnPrincipalVerified() is
  // called or when the Listener is destroyed.
  void ClearListener();

private:
  PrincipalVerifier(Listener* aListener, mozilla::ipc::PBackgroundParent* aActor,
                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);
  virtual ~PrincipalVerifier();

  void VerifyOnMainThread();
  void CompleteOnInitiatingThread();

  void DispatchToInitiatingThread(nsresult aRv);

  // Weak reference cleared by ClearListener()
  Listener* mListener;

  // set in originating thread at construction, but must be accessed and
  // released on main thread
  nsRefPtr<ContentParent> mActor;

  const mozilla::ipc::PrincipalInfo mPrincipalInfo;
  nsCOMPtr<nsIThread> mInitiatingThread;
  nsresult mResult;
  nsRefPtr<ManagerId> mManagerId;

public:
  NS_DECL_NSIRUNNABLE
};

} // namesapce cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_PrincipalVerifier_h
