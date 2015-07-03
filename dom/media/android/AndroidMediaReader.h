/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AndroidMediaReader_h_)
#define AndroidMediaReader_h_

#include "mozilla/Attributes.h"
#include "MediaResource.h"
#include "MediaDecoderReader.h"
#include "ImageContainer.h"
#include "nsAutoPtr.h"
#include "mozilla/layers/SharedRGBImage.h"
 
#include "MPAPI.h"

class nsACString;

namespace mozilla {

class AbstractMediaDecoder;

namespace layers {
class ImageContainer;
}

class AndroidMediaReader : public MediaDecoderReader
{
  nsCString mType;
  MPAPI::Decoder *mPlugin;
  bool mHasAudio;
  bool mHasVideo;
  nsIntRect mPicture;
  nsIntSize mInitialFrame;
  int64_t mVideoSeekTimeUs;
  int64_t mAudioSeekTimeUs;
  nsRefPtr<VideoData> mLastVideoFrame;
  MediaPromiseHolder<MediaDecoderReader::SeekPromise> mSeekPromise;
  MediaPromiseRequestHolder<MediaDecoderReader::VideoDataPromise> mSeekRequest;
public:
  AndroidMediaReader(AbstractMediaDecoder* aDecoder,
                     const nsACString& aContentType);

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

  virtual bool IsMediaSeekable()
  {
    // not used
    return true;
  }

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  virtual nsRefPtr<ShutdownPromise> Shutdown() override;

  class ImageBufferCallback : public MPAPI::BufferCallback {
    typedef mozilla::layers::Image Image;

  public:
    ImageBufferCallback(mozilla::layers::ImageContainer *aImageContainer);
    void *operator()(size_t aWidth, size_t aHeight,
                     MPAPI::ColorFormat aColorFormat) override;
    already_AddRefed<Image> GetImage();

  private:
    uint8_t *CreateI420Image(size_t aWidth, size_t aHeight);

    mozilla::layers::ImageContainer *mImageContainer;
    nsRefPtr<Image> mImage;
  };

};

} // namespace mozilla

#endif
