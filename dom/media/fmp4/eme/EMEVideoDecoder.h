/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMEVideoDecoder_h_
#define EMEVideoDecoder_h_

#include "GMPVideoDecoder.h"
#include "PlatformDecoderModule.h"

namespace mozilla {

class CDMProxy;
class MediaTaskQueue;

class EMEVideoCallbackAdapter : public VideoCallbackAdapter {
public:
  EMEVideoCallbackAdapter(MediaDataDecoderCallbackProxy* aCallback,
                          VideoInfo aVideoInfo,
                          layers::ImageContainer* aImageContainer)
   : VideoCallbackAdapter(aCallback, aVideoInfo, aImageContainer)
  {}

  virtual void Error(GMPErr aErr) MOZ_OVERRIDE;
};

class EMEVideoDecoder : public GMPVideoDecoder {
public:
  EMEVideoDecoder(CDMProxy* aProxy,
                  const mp4_demuxer::VideoDecoderConfig& aConfig,
                  layers::LayersBackend aLayersBackend,
                  layers::ImageContainer* aImageContainer,
                  MediaTaskQueue* aTaskQueue,
                  MediaDataDecoderCallbackProxy* aCallback)
   : GMPVideoDecoder(aConfig, aLayersBackend, aImageContainer, aTaskQueue, aCallback,
                     new EMEVideoCallbackAdapter(aCallback, VideoInfo(aConfig.display_width,
                                                                      aConfig.display_height), aImageContainer))
   , mProxy(aProxy)
  {
  }

private:
  virtual void InitTags(nsTArray<nsCString>& aTags) MOZ_OVERRIDE;
  virtual nsCString GetNodeId() MOZ_OVERRIDE;
  virtual GMPUnique<GMPVideoEncodedFrame>::Ptr CreateFrame(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;

  nsRefPtr<CDMProxy> mProxy;
};

}

#endif // EMEVideoDecoder_h_
