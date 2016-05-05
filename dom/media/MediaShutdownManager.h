/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaShutdownManager_h_)
#define MediaShutdownManager_h_

#include "nsIObserver.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

namespace mozilla {

class MediaDecoder;

// The MediaShutdownManager manages shutting down the MediaDecoder
// infrastructure in response to an xpcom-shutdown notification. This happens
// when Gecko is shutting down in the middle of operation. This is tricky, as
// there are a number of moving parts that must be shutdown in a particular
// order. Additionally the xpcom-shutdown observer *must* block until all
// threads are shutdown, which is tricky since we have a number of threads
// here and their shutdown is asynchronous. We can't have each element of
// our pipeline listening for xpcom-shutdown, as if each observer blocks
// waiting for its threads to shutdown it will block other xpcom-shutdown
// notifications from firing, and shutdown of one part of the media pipeline
// (say the State Machine thread) may depend another part to be shutdown
// first (the MediaDecoder threads). The MediaShutdownManager encapsulates
// all these dependencies, and provides a single xpcom-shutdown listener
// for the MediaDecoder infrastructure, to ensure that no shutdown order
// dependencies leak out of the MediaDecoder stack. The MediaShutdownManager
// is a singleton.
//
// The MediaShutdownManager ensures that the MediaDecoder stack is shutdown
// before returning from its xpcom-shutdown observer by keeping track of all
// the active MediaDecoders, and upon xpcom-shutdown calling Shutdown() on
// every MediaDecoder and then spinning the main thread event loop until all
// SharedThreadPools have shutdown. Once the SharedThreadPools are shutdown,
// all the state machines and their threads have been shutdown, the
// xpcom-shutdown observer returns.
//
// Note that calling the Unregister() functions may result in the singleton
// being deleted, so don't store references to the singleton, always use the
// singleton by derefing the referenced returned by
// MediaShutdownManager::Instance(), which ensures that the singleton is
// created when needed.
// i.e. like this:
//    MediaShutdownManager::Instance()::Unregister(someDecoder);
//    MediaShutdownManager::Instance()::Register(someOtherDecoder);
// Not like this:
//  MediaShutdownManager& instance = MediaShutdownManager::Instance();
//  instance.Unregister(someDecoder); // Warning! May delete instance!
//  instance.Register(someOtherDecoder); // BAD! instance may be dangling!
class MediaShutdownManager : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // The MediaShutdownManager is a singleton, access its instance with
  // this accessor.
  static MediaShutdownManager& Instance();

  // Notifies the MediaShutdownManager that it needs to track the shutdown
  // of this MediaDecoder.
  void Register(MediaDecoder* aDecoder);

  // Notifies the MediaShutdownManager that a MediaDecoder that it was
  // tracking has shutdown, and it no longer needs to be shutdown in the
  // xpcom-shutdown listener.
  void Unregister(MediaDecoder* aDecoder);

private:

  MediaShutdownManager();
  virtual ~MediaShutdownManager();

  void Shutdown();

  // Ensures we have a shutdown listener if we need one, and removes the
  // listener and destroys the singleton if we don't.
  void EnsureCorrectShutdownObserverState();

  static StaticRefPtr<MediaShutdownManager> sInstance;

  // References to the MediaDecoder. The decoders unregister themselves
  // in their Shutdown() method, so we'll drop the reference naturally when
  // we're shutting down (in the non xpcom-shutdown case).
  nsTHashtable<nsRefPtrHashKey<MediaDecoder>> mDecoders;

  // True if we have an XPCOM shutdown observer.
  bool mIsObservingShutdown;

  bool mIsDoingXPCOMShutDown;
};

} // namespace mozilla

#endif
