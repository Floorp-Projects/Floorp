/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFAudioOutputSource_h_)
#define WMFAudioOutputSource_h_

#include "WMF.h"
#include "MFTDecoder.h"
#include "mozilla/RefPtr.h"
#include "WMFMediaDataDecoder.h"

namespace mozilla {

class WMFAudioMFTManager : public MFTManager {
public:
  WMFAudioMFTManager(const AudioInfo& aConfig);
  ~WMFAudioMFTManager();

  bool Init();

  HRESULT Input(MediaRawData* aSample) override;

  // Note WMF's AAC decoder sometimes output negatively timestamped samples,
  // presumably they're the preroll samples, and we strip them. We may return
  // a null aOutput in this case.
  HRESULT Output(int64_t aStreamOffset,
                         nsRefPtr<MediaData>& aOutput) override;

  void Shutdown() override;

  TrackInfo::TrackType GetType() override {
    return TrackInfo::kAudioTrack;
  }

private:

  HRESULT UpdateOutputType();

  uint32_t mAudioChannels;
  uint32_t mAudioRate;
  nsTArray<BYTE> mUserData;

  // The offset, at which playback started since the
  // last discontinuity.
  media::TimeUnit mAudioTimeOffset;
  // The number of audio frames that we've played since the last
  // discontinuity.
  int64_t mAudioFrameSum;

  enum StreamType {
    Unknown,
    AAC,
    MP3
  };
  StreamType mStreamType;

  const GUID& GetMFTGUID();
  const GUID& GetMediaSubtypeGUID();

  // True if we need to re-initialize mAudioTimeOffset and mAudioFrameSum
  // from the next audio packet we decode. This happens after a seek, since
  // WMF doesn't mark a stream as having a discontinuity after a seek(0).
  bool mMustRecaptureAudioPosition;
};

} // namespace mozilla

#endif // WMFAudioOutputSource_h_
