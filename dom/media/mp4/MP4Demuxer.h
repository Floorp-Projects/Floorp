/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MP4Demuxer_h_)
#define MP4Demuxer_h_

#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "MediaDataDemuxer.h"
#include "MediaResource.h"

namespace mozilla {
class MP4TrackDemuxer;
class ResourceStream;

DDLoggedTypeDeclNameAndBase(MP4Demuxer, MediaDataDemuxer);

class MP4Demuxer
  : public MediaDataDemuxer
  , public DecoderDoctorLifeLogger<MP4Demuxer>
{
public:
  explicit MP4Demuxer(MediaResource* aResource);

  RefPtr<InitPromise> Init() override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  already_AddRefed<MediaTrackDemuxer>
  GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  void NotifyDataArrived() override;

  void NotifyDataRemoved() override;

private:
  friend class MP4TrackDemuxer;
  RefPtr<MediaResource> mResource;
  RefPtr<ResourceStream> mStream;
  AutoTArray<RefPtr<MP4TrackDemuxer>, 1> mAudioDemuxers;
  AutoTArray<RefPtr<MP4TrackDemuxer>, 1> mVideoDemuxers;
  nsTArray<uint8_t> mCryptoInitData;
  bool mIsSeekable;
};

} // namespace mozilla

#endif
