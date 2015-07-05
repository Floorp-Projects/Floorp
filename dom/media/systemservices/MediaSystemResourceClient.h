/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceClient_h_)
#define MediaSystemResourceClient_h_

#include "MediaSystemResourceManager.h"
#include "MediaSystemResourceTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/media/MediaSystemResourceTypes.h"
#include "mozilla/Monitor.h"
#include "nsRefPtr.h"

namespace mozilla {

class MediaSystemResourceManager;


/**
 * This is a base class for listener callbacks.
 * This callback is invoked when the media system resource reservation state
 * is changed.
 */
class MediaSystemResourceReservationListener {
public:
  virtual void ResourceReserved() = 0;
  virtual void ResourceReserveFailed() = 0;
};

/**
 * MediaSystemResourceClient is used to reserve a media system resource
 * like hw decoder. When system has a limitation of a media resource,
 * use this class to mediate use rights of the resource.
 */
class MediaSystemResourceClient
{
public:

  // Enumeration for the valid decoding states
  enum ResourceState {
    RESOURCE_STATE_START,
    RESOURCE_STATE_WAITING,
    RESOURCE_STATE_ACQUIRED,
    RESOURCE_STATE_NOT_ACQUIRED,
    RESOURCE_STATE_END
  };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSystemResourceClient)

  explicit MediaSystemResourceClient(MediaSystemResourceType aReourceType);

  bool SetListener(MediaSystemResourceReservationListener* aListener);

  // Try to acquire media resource asynchronously.
  // If the resource is used by others, wait until acquired.
  void Acquire();

  // Try to acquire media resource synchronously. If the resource is not immediately
  // available, fail to acquire it.
  // return false if resource is not acquired.
  // return true if resource is acquired.
  //
  // This function should not be called on ImageBridge thread.
  // It should be used only for compatibility with legacy code.
  bool AcquireSyncNoWait();

  void ReleaseResource();

private:
  ~MediaSystemResourceClient();

  nsRefPtr<MediaSystemResourceManager> mManager;
  const MediaSystemResourceType mResourceType;
  const uint32_t mId;

  // Modified only by MediaSystemResourceManager.
  // Accessed and modified with MediaSystemResourceManager::mReentrantMonitor held.
  MediaSystemResourceReservationListener* mListener;
  ResourceState mResourceState;
  bool mIsSync;
  ReentrantMonitor* mAcquireSyncWaitMonitor;
  bool* mAcquireSyncWaitDone;

  static mozilla::Atomic<uint32_t> sSerialCounter;

  friend class MediaSystemResourceManager;
};

} // namespace mozilla

#endif
