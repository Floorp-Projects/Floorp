/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef OMX_CODEC_PROXY_DECODER_H_
#define OMX_CODEC_PROXY_DECODER_H_


#include <android/native_window.h>
#include <media/IOMX.h>
#include <stagefright/MediaBuffer.h>
#include <stagefright/MediaSource.h>
#include <utils/threads.h>

#include "mozilla/media/MediaSystemResourceClient.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"

namespace android {

struct MetaData;

class OMXCodecProxy : public MediaSource
                    , public mozilla::MediaSystemResourceReservationListener
{
public:
  typedef mozilla::MozPromise<bool /* aIgnored */, bool /* aIgnored */, /* IsExclusive = */ true> CodecPromise;

  // Enumeration for the valid resource allcoation states
  enum class ResourceState : int8_t {
    START,
    WAITING,
    ACQUIRED,
    NOT_ACQUIRED,
    END
  };

  static sp<OMXCodecProxy> Create(
          const sp<IOMX> &omx,
          const sp<MetaData> &meta, bool createEncoder,
          const sp<MediaSource> &source,
          const char *matchComponentName = nullptr,
          uint32_t flags = 0,
          const sp<ANativeWindow> &nativeWindow = nullptr);

    RefPtr<CodecPromise> requestResource();

    // MediaSystemResourceReservationListener
    void ResourceReserved() override;
    void ResourceReserveFailed() override;

    // MediaSource
    status_t start(MetaData *params = nullptr) override;
    status_t stop() override;

    sp<MetaData> getFormat() override;

    status_t read(
            MediaBuffer **buffer, const ReadOptions *options = nullptr) override;

    status_t pause() override;

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

    RefPtr<mozilla::MediaSystemResourceClient> mResourceClient;
    ResourceState mState;
    mozilla::MozPromiseHolder<CodecPromise> mCodecPromise;
};

} // namespace android

#endif  // OMX_CODEC_PROXY_DECODER_H_
