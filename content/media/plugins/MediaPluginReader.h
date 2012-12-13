/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaPluginReader_h_)
#define MediaPluginReader_h_

#include "MediaResource.h"
#include "MediaDecoderReader.h"
#include "ImageContainer.h"
#include "mozilla/layers/SharedRGBImage.h"
 
#include "MPAPI.h"

class nsACString;

namespace mozilla {

class AbstractMediaDecoder;

namespace layers {
class ImageContainer;
}
 
class MediaPluginReader : public MediaDecoderReader
{
  nsCString mType;
  MPAPI::Decoder *mPlugin;
  bool mHasAudio;
  bool mHasVideo;
  nsIntRect mPicture;
  nsIntSize mInitialFrame;
  int64_t mVideoSeekTimeUs;
  int64_t mAudioSeekTimeUs;
  VideoData *mLastVideoFrame;
public:
  MediaPluginReader(AbstractMediaDecoder* aDecoder,
                    const nsACString& aContentType);
  ~MediaPluginReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);
  virtual nsresult ResetDecode();

  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold);

  virtual bool HasAudio()
  {
    return mHasAudio;
  }

  virtual bool HasVideo()
  {
    return mHasVideo;
  }

  virtual nsresult ReadMetadata(VideoInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime);
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered, int64_t aStartTime);
  class ImageBufferCallback : public MPAPI::BufferCallback {
    typedef mozilla::layers::Image Image;
  public:
    ImageBufferCallback(mozilla::layers::ImageContainer *aImageContainer);
    void *operator()(size_t aWidth, size_t aHeight,
                     MPAPI::ColorFormat aColorFormat);
    already_AddRefed<Image> GetImage();
  private:
    mozilla::layers::ImageContainer *mImageContainer;
    nsRefPtr<Image> mImage;
  };
};

} // namespace mozilla

#endif
