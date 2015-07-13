/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceManager_h_)
#define MediaSystemResourceManager_h_

#include <queue>

#include "MediaSystemResourceTypes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace media {
class MediaSystemResourceManagerChild;
} // namespace media

class MediaSystemResourceClient;
class MediaSystemResourceReservationListener;
class MediaTaskQueue;
class ReentrantMonitor;

/**
 * Manage media system resource allocation requests within a process.
 */
class MediaSystemResourceManager
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSystemResourceManager)

  static MediaSystemResourceManager* Get();
  static void Init();
  static void Shutdown();

  void OnIpcClosed();

  void Register(MediaSystemResourceClient* aClient);
  void Unregister(MediaSystemResourceClient* aClient);

  bool SetListener(MediaSystemResourceClient* aClient,
                   MediaSystemResourceReservationListener* aListener);

  void Acquire(MediaSystemResourceClient* aClient);
  bool AcquireSyncNoWait(MediaSystemResourceClient* aClient);
  void ReleaseResource(MediaSystemResourceClient* aClient);

  void RecvResponse(uint32_t aId, bool aSuccess);

private:
  MediaSystemResourceManager();
  virtual ~MediaSystemResourceManager();

  void OpenIPC();
  void CloseIPC();
  bool IsIpcClosed();

  void DoAcquire(uint32_t aId);

  void DoRelease(uint32_t aId);

  void HandleAcquireResult(uint32_t aId, bool aSuccess);

  ReentrantMonitor mReentrantMonitor;

  bool mShutDown;

  media::MediaSystemResourceManagerChild* mChild;

  nsDataHashtable<nsUint32HashKey, MediaSystemResourceClient*> mResourceClients;

  static StaticRefPtr<MediaSystemResourceManager> sSingleton;
};

} // namespace mozilla

#endif
