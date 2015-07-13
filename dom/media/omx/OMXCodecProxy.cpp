/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "OMXCodecProxy"

#include <binder/IPCThreadState.h>
#include <cutils/properties.h>
#include <stagefright/foundation/ADebug.h>
#include <stagefright/MetaData.h>
#include <stagefright/OMXCodec.h>
#include <utils/Log.h>

#include "nsDebug.h"

#include "OMXCodecProxy.h"

namespace android {

// static
sp<OMXCodecProxy> OMXCodecProxy::Create(
        const sp<IOMX> &omx,
        const sp<MetaData> &meta, bool createEncoder,
        const sp<MediaSource> &source,
        const char *matchComponentName,
        uint32_t flags,
        const sp<ANativeWindow> &nativeWindow)
{
  sp<OMXCodecProxy> proxy;

  const char *mime;
  if (!meta->findCString(kKeyMIMEType, &mime)) {
    return nullptr;
  }

  if (!strncasecmp(mime, "video/", 6)) {
    proxy = new OMXCodecProxy(omx, meta, createEncoder, source, matchComponentName, flags, nativeWindow);
  }
  return proxy;
}


OMXCodecProxy::OMXCodecProxy(
        const sp<IOMX> &omx,
        const sp<MetaData> &meta,
        bool createEncoder,
        const sp<MediaSource> &source,
        const char *matchComponentName,
        uint32_t flags,
        const sp<ANativeWindow> &nativeWindow)
    : mOMX(omx),
      mSrcMeta(meta),
      mComponentName(nullptr),
      mIsEncoder(createEncoder),
      mFlags(flags),
      mNativeWindow(nativeWindow),
      mSource(source),
      mState(ResourceState::START)
{
}

OMXCodecProxy::~OMXCodecProxy()
{
  mState = ResourceState::END;

  if (mOMXCodec.get()) {
    wp<MediaSource> tmp = mOMXCodec;
    mOMXCodec.clear();
    while (tmp.promote() != nullptr) {
        // this value come from stagefrigh's AwesomePlayer.
        usleep(1000);
    }
  }
  // Complete all pending Binder ipc transactions
  IPCThreadState::self()->flushCommands();

  if (mResourceClient) {
    mResourceClient->ReleaseResource();
    mResourceClient = nullptr;
  }

  mSource.clear();
  free(mComponentName);
  mComponentName = nullptr;
}

void OMXCodecProxy::setListener(const wp<CodecResourceListener>& listener)
{
  Mutex::Autolock autoLock(mLock);
  mListener = listener;
}

void OMXCodecProxy::notifyResourceReserved()
{
  sp<CodecResourceListener> listener = mListener.promote();
  if (listener != nullptr) {
    listener->codecReserved();
  }
}

void OMXCodecProxy::notifyResourceCanceled()
{
  sp<CodecResourceListener> listener = mListener.promote();
  if (listener != nullptr) {
    listener->codecCanceled();
  }
}

void OMXCodecProxy::requestResource()
{
  Mutex::Autolock autoLock(mLock);

  if (mResourceClient) {
    return;
  }
  mState = ResourceState::WAITING;

  mozilla::MediaSystemResourceType type = mIsEncoder ? mozilla::MediaSystemResourceType::VIDEO_ENCODER :
                                                 mozilla::MediaSystemResourceType::VIDEO_DECODER;
  mResourceClient = new mozilla::MediaSystemResourceClient(type);
  mResourceClient->SetListener(this);
  mResourceClient->Acquire();
}

// Called on ImageBridge thread
void
OMXCodecProxy::ResourceReserved()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::WAITING) {
    return;
  }

  const char *mime;
  if (!mSrcMeta->findCString(kKeyMIMEType, &mime)) {
    mState = ResourceState::END;
    notifyResourceCanceled();
    return;
  }

  if (!strncasecmp(mime, "video/", 6)) {
    sp<MediaSource> codec;
    mOMXCodec = OMXCodec::Create(mOMX, mSrcMeta, mIsEncoder, mSource, mComponentName, mFlags, mNativeWindow);
    if (mOMXCodec == nullptr) {
      mState = ResourceState::END;
      notifyResourceCanceled();
      return;
    }
    // Check if this video is sized such that we're comfortable
    // possibly using an OMX decoder.
    int32_t maxWidth, maxHeight;
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.moz.omx.hw.max_width", propValue, "-1");
    maxWidth = atoi(propValue);
    property_get("ro.moz.omx.hw.max_height", propValue, "-1");
    maxHeight = atoi(propValue);

    int32_t width = -1, height = -1;
    if (maxWidth > 0 && maxHeight > 0 &&
        !(mOMXCodec->getFormat()->findInt32(kKeyWidth, &width) &&
          mOMXCodec->getFormat()->findInt32(kKeyHeight, &height) &&
          width * height <= maxWidth * maxHeight)) {
      printf_stderr("Failed to get video size, or it was too large for HW decoder (<w=%d, h=%d> but <maxW=%d, maxH=%d>)",
                    width, height, maxWidth, maxHeight);
      mOMXCodec.clear();
      mState = ResourceState::END;
      notifyResourceCanceled();
      return;
    }

    if (mOMXCodec->start() != OK) {
      NS_WARNING("Couldn't start OMX video source");
      mOMXCodec.clear();
      mState = ResourceState::END;
      notifyResourceCanceled();
      return;
    }
  }

  mState = ResourceState::ACQUIRED;
  notifyResourceReserved();
}

// Called on ImageBridge thread
void
OMXCodecProxy::ResourceReserveFailed()
{
  Mutex::Autolock autoLock(mLock);
  mState = ResourceState::NOT_ACQUIRED;
}

status_t OMXCodecProxy::start(MetaData *params)
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::ACQUIRED) {
    return NO_INIT;
  }
  CHECK(mOMXCodec.get() != nullptr);
  return mOMXCodec->start();
}

status_t OMXCodecProxy::stop()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::ACQUIRED) {
    return NO_INIT;
  }
  CHECK(mOMXCodec.get() != nullptr);
  return mOMXCodec->stop();
}

sp<MetaData> OMXCodecProxy::getFormat()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::ACQUIRED) {
    sp<MetaData> meta = new MetaData;
    return meta;
  }
  CHECK(mOMXCodec.get() != nullptr);
  return mOMXCodec->getFormat();
}

status_t OMXCodecProxy::read(MediaBuffer **buffer, const ReadOptions *options)
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::ACQUIRED) {
    return NO_INIT;
  }
  CHECK(mOMXCodec.get() != nullptr);
  return mOMXCodec->read(buffer, options);
}

status_t OMXCodecProxy::pause()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != ResourceState::ACQUIRED) {
    return NO_INIT;
  }
  CHECK(mOMXCodec.get() != nullptr);
  return mOMXCodec->pause();
}

} // namespace android
