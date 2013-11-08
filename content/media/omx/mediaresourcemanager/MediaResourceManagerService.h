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
#include <utils/List.h>
#include <utils/RefBase.h>

#include "IMediaResourceManagerClient.h"
#include "IMediaResourceManagerService.h"

namespace android {

/**
 * Manage permissions of using media resources(hw decoder, hw encoder, camera)
 * XXX Current implementaion support only one hw video decoder.
 *     Need to extend to support multiple instance and other resources.
 */
class MediaResourceManagerService: public BnMediaResourceManagerService,
                                   public IBinder::DeathRecipient
{
public:
  // The maximum number of hardware decoders available.
  enum { VIDEO_DECODER_COUNT = 1 };

  enum {
    kNotifyRequest = 'noti'
  };

  // Instantiate MediaResourceManagerService and register to service manager.
  // If service manager is not present, wait until service manager becomes present.
  static  void instantiate();

  // DeathRecipient
  virtual void binderDied(const wp<IBinder>& who);

  // derived from IMediaResourceManagerService
  virtual void requestMediaResource(const sp<IMediaResourceManagerClient>& client, int resourceType);
  virtual status_t cancelClient(const sp<IMediaResourceManagerClient>& client);

  // Receive a message from AHandlerReflector.
  // Called on ALooper thread.
  void onMessageReceived(const sp<AMessage> &msg);

protected:
  MediaResourceManagerService();
  virtual ~MediaResourceManagerService();

protected:
  // Represent a media resouce.
  // Hold a IMediaResourceManagerClient that got a media resource as IBinder.
  struct ResourceSlot {
    ResourceSlot ()
      {
      }
      sp<IBinder> mClient;
    };

  void cancelClientLocked(const sp<IBinder>& binder);

  // mVideoDecoderSlots is the array of slots that represent a media resource.
  ResourceSlot mVideoDecoderSlots[VIDEO_DECODER_COUNT];
  // The maximum number of hardware decoders available on the device.
  int mVideoDecoderCount;

  // The lock protects mVideoDecoderSlots and mVideoCodecRequestQueue called
  //  from multiple threads.
  Mutex mLock;
  typedef List<sp<IBinder> > Fifo;
  // Queue of media resource requests.
  // Hold IMediaResourceManagerClient that requesting a media resource as IBinder.
  Fifo mVideoCodecRequestQueue;

  // ALooper is a message loop used in stagefright.
  // It creates a thread for messages and handles messages in the thread.
  // ALooper is a clone of Looper in android Java.
  // http://developer.android.com/reference/android/os/Looper.html
  sp<ALooper> mLooper;
  // deliver a message to a wrapped object(OmxDecoder).
  // AHandlerReflector is similar to Handler in android Java.
  // http://developer.android.com/reference/android/os/Handler.html
  sp<AHandlerReflector<MediaResourceManagerService> > mReflector;

};

}; // namespace android

#endif // ANDROID_MEDIARESOURCEMANAGERSERVICE_H
