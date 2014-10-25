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
  static TemporaryRef<SharedThreadPool> Get(const nsCString& aName,
                                            uint32_t aThreadLimit = 4);

  // Spins the event loop until all thread pools are shutdown.
  // *Must* be called on the main thread.
  static void SpinUntilShutdown();

  // We implement custom threadsafe AddRef/Release pair, that destroys the
  // the shared pool singleton when the refcount drops to 0. The addref/release
  // are implemented using locking, so it's not recommended that you use them
  // in a tight loop.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  // Forward behaviour to wrapped thread pool implementation.
  NS_FORWARD_SAFE_NSITHREADPOOL(mPool);
  NS_FORWARD_SAFE_NSIEVENTTARGET(mEventTarget);

private:

  // Creates necessary statics.
  // Main thread only.
  static void EnsureInitialized();

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
