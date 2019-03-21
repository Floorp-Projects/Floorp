/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(DAV1DDecoder_h_)
#  define DAV1DDecoder_h_

#  include "PlatformDecoderModule.h"
#  include "dav1d/dav1d.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(DAV1DDecoder, MediaDataDecoder);

typedef nsRefPtrHashtable<nsPtrHashKey<const uint8_t>, MediaRawData>
    MediaRawDataHashtable;

class DAV1DDecoder : public MediaDataDecoder,
                     public DecoderDoctorLifeLogger<DAV1DDecoder> {
 public:
  explicit DAV1DDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return NS_LITERAL_CSTRING("av1 libdav1d video decoder");
  }

  void ReleaseDataBuffer(const uint8_t* buf);

 private:
  ~DAV1DDecoder() = default;
  RefPtr<DecodePromise> InvokeDecode(MediaRawData* aSample);
  int GetPicture(DecodedData& aData, MediaResult& aResult);
  already_AddRefed<VideoData> ConstructImage(const Dav1dPicture& aPicture);

  Dav1dContext* mContext = nullptr;

  const VideoInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<layers::ImageContainer> mImageContainer;

  // Keep the buffers alive until dav1d
  // does not need them any more.
  MediaRawDataHashtable mDecodingBuffers;
};

}  // namespace mozilla

#endif  // DAV1DDecoder_h_
