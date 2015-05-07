/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Action_h
#define mozilla_dom_cache_Action_h

#include "mozilla/Atomics.h"
#include "mozilla/dom/cache/Types.h"
#include "nsISupportsImpl.h"

class mozIStorageConnection;

namespace mozilla {
namespace dom {
namespace cache {

class Action
{
public:
  class Resolver
  {
  public:
    // Note: Action must drop Resolver ref after calling Resolve()!
    // Note: Must be called on the same thread used to execute
    //       Action::RunOnTarget().
    virtual void Resolve(nsresult aRv) = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    AddRef(void) = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    Release(void) = 0;
  };

  // Class containing data that can be opportunistically shared between
  // multiple Actions running on the same thread/Context.  In theory
  // this could be abstracted to a generic key/value map, but for now
  // just explicitly provide accessors for the data we need.
  class Data
  {
  public:
    virtual mozIStorageConnection*
    GetConnection() const = 0;

    virtual void
    SetConnection(mozIStorageConnection* aConn) = 0;
  };

  // Execute operations on the target thread.  Once complete call
  // Resolver::Resolve().  This can be done sync or async.
  // Note: Action should hold Resolver ref until its ready to call Resolve().
  // Note: The "target" thread is determined when the Action is scheduled on
  //       Context.  The Action should not assume any particular thread is used.
  virtual void RunOnTarget(Resolver* aResolver, const QuotaInfo& aQuotaInfo,
                           Data* aOptionalData) = 0;

  // Called on initiating thread when the Action is canceled.  The Action is
  // responsible for calling Resolver::Resolve() as normal; either with a
  // normal error code or NS_ERROR_ABORT.  If CancelOnInitiatingThread() is
  // called after Resolve() has already occurred, then the cancel can be
  // ignored.
  //
  // Cancellation is a best effort to stop processing as soon as possible, but
  // does not guarantee the Action will not run.
  //
  // CancelOnInitiatingThread() may be called more than once.  Subsequent
  // calls should have no effect.
  //
  // Default implementation sets an internal cancellation flag that can be
  // queried with IsCanceled().
  virtual void CancelOnInitiatingThread();

  // Executed on the initiating thread and is passed the nsresult given to
  // Resolver::Resolve().
  virtual void CompleteOnInitiatingThread(nsresult aRv) { }

  // Executed on the initiating thread.  If this Action will operate on the
  // given cache ID then override this to return true.
  virtual bool MatchesCacheId(CacheId aCacheId) const { return false; }

  NS_INLINE_DECL_REFCOUNTING(cache::Action)

protected:
  Action();

  // virtual because deleted through base class pointer
  virtual ~Action();

  // Check if this Action has been canceled.  May be called from any thread,
  // but typically used from the target thread.
  bool IsCanceled() const;

private:
  // Accessible from any thread.
  Atomic<bool> mCanceled;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_Action_h
