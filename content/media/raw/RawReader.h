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
  RawReader(AbstractMediaDecoder* aDecoder);
  ~RawReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);
  virtual nsresult ResetDecode();
  virtual bool DecodeAudioData();

  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  int64_t aTimeThreshold);

  virtual bool HasAudio()
  {
    return false;
  }

  virtual bool HasVideo()
  {
    return true;
  }

  virtual nsresult ReadMetadata(VideoInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime);
  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime);

private:
  bool ReadFromResource(MediaResource *aResource, uint8_t *aBuf, uint32_t aLength);

  RawVideoHeader mMetadata;
  uint32_t mCurrentFrame;
  double mFrameRate;
  uint32_t mFrameSize;
  nsIntRect mPicture;
};

} // namespace mozilla

#endif
