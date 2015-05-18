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

  virtual void Error(GMPErr aErr) override;
};

class EMEVideoDecoder : public GMPVideoDecoder {
public:
  EMEVideoDecoder(CDMProxy* aProxy,
                  const VideoInfo& aConfig,
                  layers::LayersBackend aLayersBackend,
                  layers::ImageContainer* aImageContainer,
                  MediaTaskQueue* aTaskQueue,
                  MediaDataDecoderCallbackProxy* aCallback)
   : GMPVideoDecoder(aConfig,
                     aLayersBackend,
                     aImageContainer,
                     aTaskQueue,
                     aCallback,
                     new EMEVideoCallbackAdapter(aCallback,
                                                 VideoInfo(aConfig.mDisplay.width,
                                                           aConfig.mDisplay.height),
                                                 aImageContainer))
   , mProxy(aProxy)
  {
  }

private:
  virtual void InitTags(nsTArray<nsCString>& aTags) override;
  virtual nsCString GetNodeId() override;
  virtual GMPUniquePtr<GMPVideoEncodedFrame> CreateFrame(MediaRawData* aSample) override;

  nsRefPtr<CDMProxy> mProxy;
};

}

#endif // EMEVideoDecoder_h_
