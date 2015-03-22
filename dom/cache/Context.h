/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Context_h
#define mozilla_dom_cache_Context_h

#include "mozilla/dom/cache/Types.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIEventTarget;

namespace mozilla {
namespace dom {
namespace cache {

class Action;
class Manager;

// The Context class is RAII-style class for managing IO operations within the
// Cache.
//
// When a Context is created it performs the complicated steps necessary to
// initialize the QuotaManager.  Action objects dispatched on the Context are
// delayed until this initialization is complete.  They are then allow to
// execute on any specified thread.  Once all references to the Context are
// gone, then the steps necessary to release the QuotaManager are performed.
// Since pending Action objects reference the Context, this allows overlapping
// IO to opportunistically run without re-initializing the QuotaManager again.
//
// While the Context performs operations asynchronously on threads, all of
// methods in its public interface must be called on the same thread
// originally used to create the Context.
//
// As an invariant, all Context objects must be destroyed before permitting
// the "profile-before-change" shutdown event to complete.  This is ensured
// via the code in ShutdownObserver.cpp.
class Context final
{
public:
  static already_AddRefed<Context>
  Create(Manager* aManager, Action* aQuotaIOThreadAction);

  // Execute given action on the target once the quota manager has been
  // initialized.
  //
  // Only callable from the thread that created the Context.
  void Dispatch(nsIEventTarget* aTarget, Action* aAction);

  // Cancel any Actions running or waiting to run.  This should allow the
  // Context to be released and Listener::RemoveContext() will be called
  // when complete.
  //
  // Only callable from the thread that created the Context.
  void CancelAll();

  // Cancel any Actions running or waiting to run that operate on the given
  // cache ID.
  //
  // Only callable from the thread that created the Context.
  void CancelForCacheId(CacheId aCacheId);

private:
  class QuotaInitRunnable;
  class ActionRunnable;

  enum State
  {
    STATE_CONTEXT_INIT,
    STATE_CONTEXT_READY,
    STATE_CONTEXT_CANCELED
  };

  struct PendingAction
  {
    nsCOMPtr<nsIEventTarget> mTarget;
    nsRefPtr<Action> mAction;
  };

  explicit Context(Manager* aManager);
  ~Context();
  void DispatchAction(nsIEventTarget* aTarget, Action* aAction);
  void OnQuotaInit(nsresult aRv, const QuotaInfo& aQuotaInfo);
  void OnActionRunnableComplete(ActionRunnable* const aAction);

  nsRefPtr<Manager> mManager;
  State mState;
  QuotaInfo mQuotaInfo;
  nsTArray<PendingAction> mPendingActions;

  // weak refs since ~ActionRunnable() removes itself from this list
  nsTArray<ActionRunnable*> mActionRunnables;

public:
  NS_INLINE_DECL_REFCOUNTING(cache::Context)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_Context_h
