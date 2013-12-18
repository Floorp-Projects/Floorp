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
class StateMachineThread;

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
// first (the MediaDecoder threads). Additionally we need to not interfere
// with shutdown in the the non-xpcom-shutdown case, where we need to be able
// to recreate the State Machine thread after it's been destroyed without
// affecting the shutdown of the old State Machine thread. The
// MediaShutdownManager encapsulates all these dependencies, and provides
// a single xpcom-shutdown listener for the MediaDecoder infrastructure, to
// ensure that no shutdown order dependencies leak out of the MediaDecoder
// stack. The MediaShutdownManager is a singleton.
//
// The MediaShutdownManager ensures that the MediaDecoder stack is shutdown
// before returning from its xpcom-shutdown observer by keeping track of all
// the active MediaDecoders, and upon xpcom-shutdown calling Shutdown() on
// every MediaDecoder and then spinning the main thread event loop until the
// State Machine thread has shutdown. Once the State Machine thread has been
// shutdown, the xpcom-shutdown observer returns.
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

  // Notifies the MediaShutdownManager of a state machine thread that
  // must be tracked. Note that we track State Machine threads individually
  // as their shutdown and the construction of a new state machine thread
  // can interleave. This stores a strong ref to the state machine.
  void Register(StateMachineThread* aThread);

  // Notifies the MediaShutdownManager that a StateMachineThread that it was
  // tracking has shutdown, and it no longer needs to be shutdown in the
  // xpcom-shutdown listener. This drops the strong reference to the
  // StateMachineThread, which may destroy it.
  void Unregister(StateMachineThread* aThread);

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

  // References to the state machine threads that we're tracking shutdown
  // of. Note that although there is supposed to be a single state machine,
  // the construction and shutdown of these can interleave, so we must track
  // individual instances of the state machine threads.
  // These are strong references.
  nsTHashtable<nsRefPtrHashKey<StateMachineThread>> mStateMachineThreads;

  // True if we have an XPCOM shutdown observer.
  bool mIsObservingShutdown;

  bool mIsDoingXPCOMShutDown;
};

// A wrapper for the state machine thread. We must wrap this so that the
// state machine threads can shutdown independently from the
// StateMachineTracker, under the control of the MediaShutdownManager.
// The state machine thread is shutdown naturally when all decoders
// complete their shutdown. So if a new decoder is created just as the
// old state machine thread has shutdown, we need to be able to shutdown
// the old state machine thread independently of the StateMachineTracker
// creating a new state machine thread. Also if this happens we need to
// be able to force both state machine threads to shutdown in the
// MediaShutdownManager, which is why we maintain a set of state machine
// threads, even though there's supposed to only be one alive at once.
// This class does not enforce its own thread safety, the StateMachineTracker
// ensures thread safety when it uses the StateMachineThread.
class StateMachineThread {
public:
  StateMachineThread();
  ~StateMachineThread();

  NS_INLINE_DECL_REFCOUNTING(StateMachineThread);

  // Creates the wrapped thread.
  nsresult Init();

  // Returns a reference to the underlying thread. Don't shut this down
  // directly, use StateMachineThread::Shutdown() instead.
  nsIThread* GetThread();

  // Dispatches an event to the main thread to shutdown the wrapped thread.
  // The owner's (StateMachineTracker's) reference to the StateMachineThread
  // can be dropped, the StateMachineThread will shutdown itself
  // asynchronously.
  void Shutdown();

  // Processes events on the main thread event loop until this thread
  // has been shutdown. Use this to block until the asynchronous shutdown
  // has complete.
  void SpinUntilShutdownComplete();

private:
  void ShutdownThread();
  nsCOMPtr<nsIThread> mThread;
};

} // namespace mozilla

#endif
