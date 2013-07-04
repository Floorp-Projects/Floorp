/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef OMX_CODEC_PROXY_DECODER_H_
#define OMX_CODEC_PROXY_DECODER_H_


#include <android/native_window.h>
#include <IOMX.h>
#include <stagefright/MediaBuffer.h>
#include <stagefright/MediaSource.h>
#include <utils/threads.h>

#include "MediaResourceManagerClient.h"

namespace android {

struct MediaBufferGroup;
struct MetaData;

class OMXCodecProxy : public MediaSource,
                      public MediaResourceManagerClient::EventListener
{
public:
  struct EventListener : public virtual RefBase {
    virtual void statusChanged() = 0;
  };

  static sp<OMXCodecProxy> Create(
          const sp<IOMX> &omx,
          const sp<MetaData> &meta, bool createEncoder,
          const sp<MediaSource> &source,
          const char *matchComponentName = NULL,
          uint32_t flags = 0,
          const sp<ANativeWindow> &nativeWindow = NULL);

    MediaResourceManagerClient::State getState();

    void setEventListener(const wp<EventListener>& listener);

    void requestResource();
    bool IsWaitingResources();

    // MediaResourceManagerClient::EventListener
    virtual void statusChanged(int event);

    // MediaSource
    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

    virtual status_t pause();

protected:
    OMXCodecProxy(
        const sp<IOMX> &omx,
        const sp<MetaData> &meta,
        bool createEncoder,
        const sp<MediaSource> &source,
        const char *matchComponentName,
        uint32_t flags,
        const sp<ANativeWindow> &nativeWindow);

    virtual ~OMXCodecProxy();

    void notifyStatusChangedLocked();

private:
    OMXCodecProxy(const OMXCodecProxy &);
    OMXCodecProxy &operator=(const OMXCodecProxy &);

    Mutex mLock;

    sp<IOMX> mOMX;
    sp<MetaData> mSrcMeta;
    char *mComponentName;
    bool mIsEncoder;
    // Flags specified in the creation of the codec.
    uint32_t mFlags;
    sp<ANativeWindow> mNativeWindow;

    sp<MediaSource> mSource;

    sp<MediaSource> mOMXCodec;
    sp<MediaResourceManagerClient> mClient;
    MediaResourceManagerClient::State mState;

    sp<IMediaResourceManagerService> mManagerService;
    wp<OMXCodecProxy::EventListener> mEventListener;
};

}  // namespace android

#endif  // OMX_CODEC_PROXY_DECODER_H_
