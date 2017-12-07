/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(DummyMediaDataDecoder_h_)
#define DummyMediaDataDecoder_h_

#include "MediaInfo.h"
#include "mozilla/UniquePtr.h"
#include "PlatformDecoderModule.h"
#include "ReorderQueue.h"

namespace mozilla {

class MediaRawData;

class DummyDataCreator
{
public:
  virtual ~DummyDataCreator();
  virtual already_AddRefed<MediaData> Create(MediaRawData* aSample) = 0;
};

DDLoggedTypeDeclNameAndBase(DummyMediaDataDecoder, MediaDataDecoder);

// Decoder that uses a passed in object's Create function to create Null
// MediaData objects.
class DummyMediaDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<DummyMediaDataDecoder>
{
public:
  DummyMediaDataDecoder(UniquePtr<DummyDataCreator>&& aCreator,
                        const nsACString& aDescription,
                        const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;

  RefPtr<ShutdownPromise> Shutdown() override;

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;

  RefPtr<DecodePromise> Drain() override;

  RefPtr<FlushPromise> Flush() override;

  nsCString GetDescriptionName() const override;

  ConversionRequired NeedsConversion() const override;

private:
  UniquePtr<DummyDataCreator> mCreator;
  const bool mIsH264;
  const uint32_t mMaxRefFrames;
  ReorderQueue mReorderQueue;
  TrackInfo::TrackType mType;
  nsCString mDescription;
};

} // namespace mozilla

#endif // !defined(DummyMediaDataDecoder_h_)
