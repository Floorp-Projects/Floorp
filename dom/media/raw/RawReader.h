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
  virtual nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;
  virtual nsresult ResetDecode() MOZ_OVERRIDE;
  virtual bool DecodeAudioData() MOZ_OVERRIDE;

  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  int64_t aTimeThreshold) MOZ_OVERRIDE;

  virtual bool HasAudio() MOZ_OVERRIDE
  {
    return false;
  }

  virtual bool HasVideo() MOZ_OVERRIDE
  {
    return true;
  }

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) MOZ_OVERRIDE;
  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) MOZ_OVERRIDE;

  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered) MOZ_OVERRIDE;

  virtual bool IsMediaSeekable() MOZ_OVERRIDE;

private:
  bool ReadFromResource(MediaResource *aResource, uint8_t *aBuf, uint32_t aLength);

  nsresult SeekInternal(int64_t aTime);

  RawVideoHeader mMetadata;
  uint32_t mCurrentFrame;
  double mFrameRate;
  uint32_t mFrameSize;
  nsIntRect mPicture;
};

} // namespace mozilla

#endif
