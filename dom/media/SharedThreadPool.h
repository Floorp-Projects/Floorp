/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedThreadPool_h_
#define SharedThreadPool_h_

#include <queue>
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nsIThreadPool.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"

namespace mozilla {

// Wrapper that makes an nsIThreadPool a singleton, and provides a
// consistent threadsafe interface to get instances. Callers simply get a
// SharedThreadPool by the name of its nsIThreadPool. All get requests of
// the same name get the same SharedThreadPool. Users must store a reference
// to the pool, and when the last reference to a SharedThreadPool is dropped
// the pool is shutdown and deleted. Users aren't required to manually
// shutdown the pool, and can release references on any thread. On Windows
// all threads in the pool have MSCOM initialized with COINIT_MULTITHREADED.
class SharedThreadPool : public nsIThreadPool
{
public:

  // Gets (possibly creating) the shared thread pool singleton instance with
  // thread pool named aName.
  // *Must* be called on the main thread.
  static already_AddRefed<SharedThreadPool> Get(const nsCString& aName,
                                            uint32_t aThreadLimit = 4);

  // We implement custom threadsafe AddRef/Release pair, that destroys the
  // the shared pool singleton when the refcount drops to 0. The addref/release
  // are implemented using locking, so it's not recommended that you use them
  // in a tight loop.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override;

  // Forward behaviour to wrapped thread pool implementation.
  NS_FORWARD_SAFE_NSITHREADPOOL(mPool);

  // See bug 1155059 - MSVC forces us to not declare Dispatch normally in idl
  //  NS_FORWARD_SAFE_NSIEVENTTARGET(mEventTarget);
  nsresult Dispatch(nsIRunnable *event, uint32_t flags) { return !mEventTarget ? NS_ERROR_NULL_POINTER : mEventTarget->Dispatch(event, flags); }
 
  NS_IMETHOD DispatchFromScript(nsIRunnable *event, uint32_t flags) override {
      return Dispatch(event, flags);
  }

  NS_IMETHOD Dispatch(already_AddRefed<nsIRunnable>&& event, uint32_t flags) override
    { return !mEventTarget ? NS_ERROR_NULL_POINTER : mEventTarget->Dispatch(Move(event), flags); }

  NS_IMETHOD IsOnCurrentThread(bool *_retval) override { return !mEventTarget ? NS_ERROR_NULL_POINTER : mEventTarget->IsOnCurrentThread(_retval); }

  // Creates necessary statics. Called once at startup.
  static void InitStatics();

  // Spins the event loop until all thread pools are shutdown.
  // *Must* be called on the main thread.
  static void SpinUntilEmpty();

private:

  // Returns whether there are no pools in existence at the moment.
  static bool IsEmpty();

  // Creates a singleton SharedThreadPool wrapper around aPool.
  // aName is the name of the aPool, and is used to lookup the
  // SharedThreadPool in the hash table of all created pools.
  SharedThreadPool(const nsCString& aName,
                   nsIThreadPool* aPool);
  virtual ~SharedThreadPool();

  nsresult EnsureThreadLimitIsAtLeast(uint32_t aThreadLimit);

  // Name of mPool.
  const nsCString mName;

  // Thread pool being wrapped.
  nsCOMPtr<nsIThreadPool> mPool;

  // Refcount. We implement custom ref counting so that the thread pool is
  // shutdown in a threadsafe manner and singletonness is preserved.
  nsrefcnt mRefCnt;

  // mPool QI'd to nsIEventTarget. We cache this, so that we can use
  // NS_FORWARD_SAFE_NSIEVENTTARGET above.
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

} // namespace mozilla

#endif // SharedThreadPool_h_
