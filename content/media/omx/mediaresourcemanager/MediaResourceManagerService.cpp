/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaResourceManagerService"

#include <binder/IServiceManager.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/Log.h>

#include "MediaResourceManagerClient.h"
#include "MediaResourceManagerService.h"

namespace android {

int waitBeforeAdding(const android::String16& serviceName)
{
  android::sp<android::IServiceManager> sm = android::defaultServiceManager();
  for ( int i = 0 ; i < 5; i++ ) {
    if ( sm->checkService ( serviceName ) != NULL ) {
      sleep(1);
    }
    else {
      //good to go;
      return 0;
    }
  }
  // time out failure
 return -1;
}

// Wait until service manager is started
void
waitServiceManager()
{
  android::sp<android::IServiceManager> sm;
  do {
    sm = android::defaultServiceManager();
    if (sm.get()) {
      break;
    }
    usleep(50000); // 0.05 s
  } while(true);
}

/* static */
void MediaResourceManagerService::instantiate() {
  waitServiceManager();
  waitBeforeAdding( android::String16("media.resource_manager") );

  defaultServiceManager()->addService(
            String16("media.resource_manager"), new MediaResourceManagerService());
}

MediaResourceManagerService::MediaResourceManagerService()
  : mVideoDecoderCount(VIDEO_DECODER_COUNT)
{
  mLooper = new ALooper;
  mLooper->setName("MediaResourceManagerService");

  mReflector = new AHandlerReflector<MediaResourceManagerService>(this);
  // Register AMessage handler to ALooper.
  mLooper->registerHandler(mReflector);
  // Start ALooper thread.
  mLooper->start();
}

MediaResourceManagerService::~MediaResourceManagerService()
{
  // Unregister AMessage handler from ALooper.
  mLooper->unregisterHandler(mReflector->id());
  // Stop ALooper thread.
  mLooper->stop();
}

void MediaResourceManagerService::binderDied(const wp<IBinder>& who)
{
  if (who != NULL) {
    sp<IBinder> binder = who.promote();
    if (binder != NULL) {
      cancelClientLocked(binder);
    }
  }
}

void MediaResourceManagerService::requestMediaResource(const sp<IMediaResourceManagerClient>& client, int resourceType)
{
  if (resourceType != MediaResourceManagerClient::HW_VIDEO_DECODER) {
    // Support only HW_VIDEO_DECODER
    return;
  }

  {
    Mutex::Autolock autoLock(mLock);
    sp<IBinder> binder = client->asBinder();
    mVideoCodecRequestQueue.push_back(binder);
    binder->linkToDeath(this);
  }

  sp<AMessage> notify =
            new AMessage(kNotifyRequest, mReflector->id());
  // Post AMessage to MediaResourceManagerService via ALooper.
  notify->post();
}

status_t MediaResourceManagerService::cancelClient(const sp<IMediaResourceManagerClient>& client)
{
  {
    Mutex::Autolock autoLock(mLock);
    sp<IBinder> binder = client->asBinder();
    cancelClientLocked(binder);
  }

  sp<AMessage> notify =
            new AMessage(kNotifyRequest, mReflector->id());
  // Post AMessage to MediaResourceManagerService via ALooper.
  notify->post();

  return NO_ERROR;
}

// Called on ALooper thread.
void MediaResourceManagerService::onMessageReceived(const sp<AMessage> &msg)
{
  Mutex::Autolock autoLock(mLock);

  // Exit if no request.
  if (mVideoCodecRequestQueue.empty()) {
    return;
  }

  // Check if resource is available
  int found = -1;
  for (int i=0 ; i<mVideoDecoderCount ; i++) {
    if (!mVideoDecoderSlots[i].mClient.get()) {
      found = i;
    }
  }

  // Exit if no resource is available.
  if (found == -1) {
    return;
  }

  // Assign resource to IMediaResourceManagerClient
  Fifo::iterator front(mVideoCodecRequestQueue.begin());
  mVideoDecoderSlots[found].mClient = *front;
  mVideoCodecRequestQueue.erase(front);
  // Notify resource assignment to the client.
  sp<IMediaResourceManagerClient> client = interface_cast<IMediaResourceManagerClient>(mVideoDecoderSlots[found].mClient);
  client->statusChanged(MediaResourceManagerClient::CLIENT_STATE_RESOURCE_ASSIGNED);
}

void MediaResourceManagerService::cancelClientLocked(const sp<IBinder>& binder)
{
  // Clear the request from request queue.
  Fifo::iterator it(mVideoCodecRequestQueue.begin());
  while (it != mVideoCodecRequestQueue.end()) {
    if ((*it).get() == binder.get()) {
      mVideoCodecRequestQueue.erase(it);
      break;
    }
    it++;
  }

  // Clear the client from the resource
  for (int i=0 ; i<mVideoDecoderCount ; i++) {
    if (mVideoDecoderSlots[i].mClient.get() == binder.get()) {
      mVideoDecoderSlots[i].mClient = NULL;
    }
  }
  binder->unlinkToDeath(this);
}

}; // namespace android

