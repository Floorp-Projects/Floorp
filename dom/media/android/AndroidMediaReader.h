/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AndroidMediaReader_h_)
#define AndroidMediaReader_h_

#include "ImageContainer.h"
#include "MediaContentType.h"
#include "MediaDecoderReader.h"
#include "MediaResource.h"
#include "mozilla/Attributes.h"
#include "mozilla/layers/SharedRGBImage.h"

#include "MPAPI.h"

namespace mozilla {

class AbstractMediaDecoder;

namespace layers {
class ImageContainer;
}

class AndroidMediaReader : public MediaDecoderReader
{
  MediaContentType mType;
  MPAPI::Decoder *mPlugin;
  bool mHasAudio;
  bool mHasVideo;
  nsIntRect mPicture;
  nsIntSize mInitialFrame;
  int64_t mVideoSeekTimeUs;
  int64_t mAudioSeekTimeUs;
  RefPtr<VideoData> mLastVideoFrame;
  MozPromiseHolder<MediaDecoderReader::SeekPromise> mSeekPromise;
  MozPromiseRequestHolder<MediaDecoderReader::MediaDataPromise> mSeekRequest;
public:
  AndroidMediaReader(AbstractMediaDecoder* aDecoder,
                     const MediaContentType& aContentType);

  nsresult ResetDecode(TrackSet aTracks = TrackSet(TrackInfo::kAudioTrack,
                                                   TrackInfo::kVideoTrack)) override;

  bool DecodeAudioData() override;
  bool DecodeVideoFrame(bool &aKeyframeSkip, int64_t aTimeThreshold) override;

  nsresult ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags) override;
  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget) override;

  RefPtr<ShutdownPromise> Shutdown() override;

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
    RefPtr<Image> mImage;
  };

};

} // namespace mozilla

#endif
