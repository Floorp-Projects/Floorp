/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFAudioOutputSource_h_)
#  define WMFAudioOutputSource_h_

#  include "MFTDecoder.h"
#  include "WMF.h"
#  include "WMFMediaDataDecoder.h"
#  include "mozilla/RefPtr.h"

namespace mozilla {

class WMFAudioMFTManager : public MFTManager {
 public:
  explicit WMFAudioMFTManager(const AudioInfo& aConfig);
  ~WMFAudioMFTManager();

  bool Init();

  HRESULT Input(MediaRawData* aSample) override;

  // Note WMF's AAC decoder sometimes output negatively timestamped samples,
  // presumably they're the preroll samples, and we strip them. We may return
  // a null aOutput in this case.
  HRESULT Output(int64_t aStreamOffset, RefPtr<MediaData>& aOutput) override;

  void Shutdown() override;

  TrackInfo::TrackType GetType() override { return TrackInfo::kAudioTrack; }

  nsCString GetDescriptionName() const override {
    return "wmf audio decoder"_ns;
  }

 private:
  HRESULT UpdateOutputType();

  bool IsPartialOutput(const media::TimeUnit& aNewOutputDuration,
                       const bool aIsRateChangedToHigher) const;

  uint32_t mAudioChannels;
  AudioConfig::ChannelLayout::ChannelMap mChannelsMap;
  uint32_t mAudioRate;
  nsTArray<BYTE> mUserData;

  enum StreamType { Unknown, AAC, MP3 };
  StreamType mStreamType;

  const GUID& GetMFTGUID();
  const GUID& GetMediaSubtypeGUID();

  media::TimeUnit mLastInputTime = media::TimeUnit::Zero();
  media::TimeUnit mLastOutputDuration = media::TimeUnit::Zero();

  bool mFirstFrame = true;
};

}  // namespace mozilla

#endif  // WMFAudioOutputSource_h_
