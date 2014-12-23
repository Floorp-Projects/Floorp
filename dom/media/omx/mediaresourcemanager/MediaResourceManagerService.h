/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ANDROID_MEDIARESOURCEMANAGERSERVICE_H
#define ANDROID_MEDIARESOURCEMANAGERSERVICE_H

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>

#include "IMediaResourceManagerClient.h"
#include "IMediaResourceManagerService.h"

namespace android {

/**
 * Manage permissions of using media resources(hw decoder, hw encoder, camera)
 * XXX Current implementation supports only one hw video codec.
 *     Need to extend to support multiple instance and other resources.
 */
class MediaResourceManagerService: public BnMediaResourceManagerService,
                                   public IBinder::DeathRecipient
{
public:
  // The maximum number of hardware resoureces available.
  enum
  {
    VIDEO_DECODER_COUNT = 1,
    VIDEO_ENCODER_COUNT = 1
  };

  enum
  {
    kNotifyRequest = 'noti',
  };

  static const char* kMsgKeyResourceType;

  // Instantiate MediaResourceManagerService and register to service manager.
  // If service manager is not present, wait until service manager becomes present.
  static  void instantiate();

  // DeathRecipient
  virtual void binderDied(const wp<IBinder>& who);

  // derived from IMediaResourceManagerService
  virtual status_t requestMediaResource(const sp<IMediaResourceManagerClient>& client,
                                        int resourceType, bool willWait);
  virtual status_t cancelClient(const sp<IMediaResourceManagerClient>& client,
                                int resourceType);

  // Receive a message from AHandlerReflector.
  // Called on ALooper thread.
  void onMessageReceived(const sp<AMessage> &msg);

protected:
  MediaResourceManagerService();
  virtual ~MediaResourceManagerService();

private:
  // Represent a media resouce.
  // Hold a IMediaResourceManagerClient that got a media resource as IBinder.
  struct ResourceSlot
  {
    sp<IBinder> mClient;
  };
  typedef Vector<ResourceSlot> Slots;

  typedef List<sp<IBinder> > Fifo;
  struct Resources
  {
    // Queue of media resource requests. Hold IMediaResourceManagerClient that
    // requesting a media resource as IBinder.
    Fifo mRequestQueue;
    // All resources that can be requested. Hold |ResourceSlot|s that track
    // their usage.
    Slots mSlots;
  };

  typedef KeyedVector<ResourceType, Resources> ResourcesMap;
  // Manages requests from clients and availability of resources.
  class ResourceTable
  {
    ResourceTable();
    ~ResourceTable();
    // Resource operations.
    bool supportsType(ResourceType type);
    ssize_t findAvailableResource(ResourceType type, size_t number_needed = 1);
    bool isOwnedByClient(const sp<IBinder>& client, ResourceType type, size_t index);
    status_t aquireResource(const sp<IBinder>& client, ResourceType type, size_t index);
    ResourceSlot* resourceOfTypeAt(ResourceType type, size_t index);
    // Request operations.
    bool hasRequest(ResourceType type);
    uint32_t countRequests(ResourceType type);
    const sp<IBinder>& nextRequest(ResourceType type);
    status_t enqueueRequest(const sp<IBinder>& client, ResourceType type);
    status_t dequeueRequest(ResourceType type);
    status_t forgetClient(const sp<IBinder>& client, ResourceType type);
    status_t forgetClient(const sp<IBinder>& client);

    friend class MediaResourceManagerService;

    // A map for all types of supported resources.
    ResourcesMap mMap;
  };

  void cancelClientLocked(const sp<IBinder>& binder, ResourceType resourceType);

  // ALooper is a message loop used in stagefright.
  // It creates a thread for messages and handles messages in the thread.
  // ALooper is a clone of Looper in android Java.
  // http://developer.android.com/reference/android/os/Looper.html
  sp<ALooper> mLooper;
  // deliver a message to a wrapped object(OmxDecoder).
  // AHandlerReflector is similar to Handler in android Java.
  // http://developer.android.com/reference/android/os/Handler.html
  sp<AHandlerReflector<MediaResourceManagerService> > mReflector;

  // The lock protects manager operations called from multiple threads.
  Mutex mLock;

  // Keeps all the records.
  ResourceTable mResources;
};

}; // namespace android

#endif // ANDROID_MEDIARESOURCEMANAGERSERVICE_H
