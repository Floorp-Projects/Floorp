/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaShutdownManager_h_)
#define MediaShutdownManager_h_

#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsIAsyncShutdown.h"
#include "nsIThread.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {

class MediaDecoder;

// The MediaShutdownManager manages shutting down the MediaDecoder
// infrastructure in response to an xpcom-shutdown notification.
// This happens when Gecko is shutting down in the middle of operation.
// This is tricky, as there are a number of moving parts that must
// be shutdown in a particular order. The MediaShutdownManager
// encapsulates all these dependencies to ensure that no shutdown
// order dependencies leak out of the MediaDecoder stack.
// The MediaShutdownManager is a singleton.
//
// The MediaShutdownManager ensures that the MediaDecoder stack
// is shutdown before exiting xpcom-shutdown stage by registering
// itself with nsIAsyncShutdownService to receive notification
// when the stage of shutdown has started and then calls Shutdown()
// on every MediaDecoder. Shutdown will not proceed until all
// MediaDecoders finish shutdown and MediaShutdownManager unregisters
// itself from the async shutdown service.
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
class MediaShutdownManager : public nsIAsyncShutdownBlocker {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  static void InitStatics();

  // The MediaShutdownManager is a singleton, access its instance with
  // this accessor.
  static MediaShutdownManager& Instance();

  // Notifies the MediaShutdownManager that it needs to track the shutdown
  // of this MediaDecoder.
  nsresult Register(MediaDecoder* aDecoder);

  // Notifies the MediaShutdownManager that a MediaDecoder that it was
  // tracking has shutdown, and it no longer needs to be shutdown in the
  // xpcom-shutdown listener.
  void Unregister(MediaDecoder* aDecoder);

private:
  enum InitPhase
  {
    NotInited,
    InitSucceeded,
    InitFailed,
    XPCOMShutdownStarted,
    XPCOMShutdownEnded
  };

  static InitPhase sInitPhase;

  MediaShutdownManager();
  virtual ~MediaShutdownManager();
  void RemoveBlocker();

  static StaticRefPtr<MediaShutdownManager> sInstance;

  // References to the MediaDecoder. The decoders unregister themselves
  // in their Shutdown() method, so we'll drop the reference naturally when
  // we're shutting down (in the non xpcom-shutdown case).
  nsTHashtable<nsRefPtrHashKey<MediaDecoder>> mDecoders;
};

} // namespace mozilla

#endif
