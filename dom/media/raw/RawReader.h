/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RawReader_h_)
#define RawReader_h_

#include "MediaDecoderReader.h"
#include "RawStructs.h"

namespace mozilla {

class RawReader : public MediaDecoderReader
{
public:
  explicit RawReader(AbstractMediaDecoder* aDecoder);

protected:
  ~RawReader();

public:
  nsresult ResetDecode(TrackSet aTracks) override;
  bool DecodeAudioData() override;

  bool DecodeVideoFrame(bool &aKeyframeSkip,
                        int64_t aTimeThreshold) override;

  nsresult ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags) override;
  RefPtr<SeekPromise> Seek(SeekTarget aTarget, int64_t aEndTime) override;

  media::TimeIntervals GetBuffered() override;

private:
  bool ReadFromResource(uint8_t *aBuf, uint32_t aLength);

  RawVideoHeader mMetadata;
  uint32_t mCurrentFrame;
  double mFrameRate;
  uint32_t mFrameSize;
  nsIntRect mPicture;
  MediaResourceIndex mResource;
};

} // namespace mozilla

#endif
