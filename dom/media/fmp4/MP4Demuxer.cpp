/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "MP4Demuxer.h"
#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/ResourceStream.h"
#include "mp4_demuxer/BufferStream.h"

namespace mozilla {

MP4Demuxer::MP4Demuxer(MediaResource* aResource)
  : mResource(aResource)
  , mStream(new mp4_demuxer::ResourceStream(aResource))
  , mInitData(new MediaLargeByteBuffer)
{
}

nsRefPtr<MP4Demuxer::InitPromise>
MP4Demuxer::Init()
{
  AutoPinned<mp4_demuxer::ResourceStream> stream(mStream);

  // Check that we have enough data to read the metadata.
  MediaByteRange br = mp4_demuxer::MP4Metadata::MetadataRange(stream);
  if (br.IsNull()) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::WAITING_FOR_DATA, __func__);
  }

  if (!mInitData->SetLength(br.Length())) {
    // OOM
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  size_t size;
  mStream->ReadAt(br.mStart, mInitData->Elements(), br.Length(), &size);
  if (size != size_t(br.Length())) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  nsRefPtr<mp4_demuxer::BufferStream> bufferstream =
    new mp4_demuxer::BufferStream(mInitData);

  mMetadata = MakeUnique<mp4_demuxer::MP4Metadata>(bufferstream);

  if (!mMetadata->GetNumberTracks(mozilla::TrackInfo::kAudioTrack) &&
      !mMetadata->GetNumberTracks(mozilla::TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

already_AddRefed<MediaDataDemuxer>
MP4Demuxer::Clone() const
{
  nsRefPtr<MP4Demuxer> demuxer = new MP4Demuxer(mResource);
  demuxer->mInitData = mInitData;
  nsRefPtr<mp4_demuxer::BufferStream> bufferstream =
    new mp4_demuxer::BufferStream(mInitData);
  demuxer->mMetadata = MakeUnique<mp4_demuxer::MP4Metadata>(bufferstream);
  if (!mMetadata->GetNumberTracks(mozilla::TrackInfo::kAudioTrack) &&
      !mMetadata->GetNumberTracks(mozilla::TrackInfo::kVideoTrack)) {
    NS_WARNING("Couldn't recreate MP4Demuxer");
    return nullptr;
  }
  return demuxer.forget();
}

bool
MP4Demuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return !!GetNumberTracks(aType);
}

uint32_t
MP4Demuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  return mMetadata->GetNumberTracks(aType);
}

already_AddRefed<MediaTrackDemuxer>
MP4Demuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (mMetadata->GetNumberTracks(aType) <= aTrackNumber) {
    return nullptr;
  }
  nsRefPtr<MP4TrackDemuxer> e =
    new MP4TrackDemuxer(this, aType, aTrackNumber);
  mDemuxers.AppendElement(e);

  return e.forget();
}

bool
MP4Demuxer::IsSeekable() const
{
  return mMetadata->CanSeek();
}

void
MP4Demuxer::NotifyDataArrived(uint32_t aLength, int64_t aOffset)
{
  for (uint32_t i = 0; i < mDemuxers.Length(); i++) {
    mDemuxers[i]->NotifyDataArrived();
  }
}

UniquePtr<EncryptionInfo>
MP4Demuxer::GetCrypto()
{
  const mp4_demuxer::CryptoFile& cryptoFile = mMetadata->Crypto();
  if (!cryptoFile.valid) {
    return nullptr;
  }

  const nsTArray<mp4_demuxer::PsshInfo>& psshs = cryptoFile.pssh;
  nsTArray<uint8_t> initData;
  for (uint32_t i = 0; i < psshs.Length(); i++) {
    initData.AppendElements(psshs[i].data);
  }

  if (initData.IsEmpty()) {
    return nullptr;
  }

  auto crypto = MakeUnique<EncryptionInfo>();
  crypto->AddInitData(NS_LITERAL_STRING("cenc"), Move(initData));

  return crypto;
}

MP4TrackDemuxer::MP4TrackDemuxer(MP4Demuxer* aParent,
                                 TrackInfo::TrackType aType,
                                 uint32_t aTrackNumber)
  : mParent(aParent)
  , mStream(new mp4_demuxer::ResourceStream(mParent->mResource))
  , mMonitor("MP4TrackDemuxer")
{
  mInfo = mParent->mMetadata->GetTrackInfo(aType, aTrackNumber);

  MOZ_ASSERT(mInfo);

  nsTArray<mp4_demuxer::Index::Indice> indices;
  if (!mParent->mMetadata->ReadTrackIndex(indices, mInfo->mTrackId)) {
    MOZ_ASSERT(false);
  }
  mIndex = new mp4_demuxer::Index(indices,
                                  mStream,
                                  mInfo->mTrackId,
                                  mInfo->IsAudio(),
                                  &mMonitor);
  mIterator = MakeUnique<mp4_demuxer::SampleIterator>(mIndex);
  NotifyDataArrived(); // Force update of index
}

UniquePtr<TrackInfo>
MP4TrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

nsRefPtr<MP4TrackDemuxer::SeekPromise>
MP4TrackDemuxer::Seek(media::TimeUnit aTime)
{
  int64_t seekTime = aTime.ToMicroseconds();
  mQueuedSample = nullptr;

  MonitorAutoLock mon(mMonitor);
  mIterator->Seek(seekTime);

  // Check what time we actually seeked to.
  mQueuedSample = mIterator->GetNext();
  if (mQueuedSample) {
    seekTime = mQueuedSample->mTime;
  }

  return SeekPromise::CreateAndResolve(media::TimeUnit::FromMicroseconds(seekTime), __func__);
}

nsRefPtr<MP4TrackDemuxer::SamplesPromise>
MP4TrackDemuxer::GetSamples(int32_t aNumSamples)
{
  nsRefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  if (mQueuedSample) {
    samples->mSamples.AppendElement(mQueuedSample);
    mQueuedSample = nullptr;
    aNumSamples--;
  }
  MonitorAutoLock mon(mMonitor);
  nsRefPtr<MediaRawData> sample;
  while (aNumSamples && (sample = mIterator->GetNext())) {
    samples->mSamples.AppendElement(sample);
    aNumSamples--;
  }

  if (samples->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::END_OF_STREAM, __func__);
  } else {
    UpdateSamples(samples->mSamples);
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
MP4TrackDemuxer::Reset()
{
  mQueuedSample = nullptr;
  // TODO, Seek to first frame available, which isn't always 0.
  MonitorAutoLock mon(mMonitor);
  mIterator->Seek(0);
}

void
MP4TrackDemuxer::UpdateSamples(nsTArray<nsRefPtr<MediaRawData>>& aSamples)
{
  for (size_t i = 0; i < aSamples.Length(); i++) {
    MediaRawData* sample = aSamples[i];
    if (sample->mCrypto.mValid) {
      nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
      writer->mCrypto.mMode = mInfo->mCrypto.mMode;
      writer->mCrypto.mIVSize = mInfo->mCrypto.mIVSize;
      writer->mCrypto.mKeyId.AppendElements(mInfo->mCrypto.mKeyId);
    }
    if (mInfo->GetAsVideoInfo()) {
      sample->mExtraData = mInfo->GetAsVideoInfo()->mExtraData;
    }
  }
  if (mNextKeyframeTime.isNothing() ||
      aSamples.LastElement()->mTime >= mNextKeyframeTime.value().ToMicroseconds()) {
    mNextKeyframeTime.reset();
    mp4_demuxer::Microseconds frameTime = mIterator->GetNextKeyframeTime();
    if (frameTime != -1) {
      mNextKeyframeTime.emplace(
        media::TimeUnit::FromMicroseconds(frameTime));
    }
  }
}

nsresult
MP4TrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime =
      media::TimeUnit::FromMicroseconds(std::numeric_limits<int64_t>::max());
  } else {
    *aTime = mNextKeyframeTime.value();
  }
  return NS_OK;
}

nsRefPtr<MP4TrackDemuxer::SkipAccessPointPromise>
MP4TrackDemuxer::SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold)
{
  MonitorAutoLock mon(mMonitor);
  mQueuedSample = nullptr;
  // Loop until we reach the next keyframe after the threshold.
  uint32_t parsed = 0;
  bool found = false;
  nsRefPtr<MediaRawData> sample;
  while (!found && (sample = mIterator->GetNext())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold.ToMicroseconds()) {
      found = true;
      mQueuedSample = sample;
    }
  }
  if (found) {
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(DemuxerFailureReason::END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

int64_t
MP4TrackDemuxer::GetEvictionOffset(media::TimeUnit aTime)
{
  MonitorAutoLock mon(mMonitor);
  return int64_t(mIndex->GetEvictionOffset(aTime.ToMicroseconds()));
}

media::TimeIntervals
MP4TrackDemuxer::GetBuffered()
{
  AutoPinned<MediaResource> resource(mParent->mResource);
  nsTArray<MediaByteRange> byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);

  if (NS_FAILED(rv)) {
    return media::TimeIntervals();
  }
  nsTArray<mp4_demuxer::Interval<int64_t>> timeRanges;

  MonitorAutoLock mon(mMonitor);
  int64_t endComposition =
    mIndex->GetEndCompositionIfBuffered(byteRanges);

  mIndex->ConvertByteRangesToTimeRanges(byteRanges, &timeRanges);
  if (endComposition) {
    mp4_demuxer::Interval<int64_t>::SemiNormalAppend(
      timeRanges, mp4_demuxer::Interval<int64_t>(endComposition, endComposition));
  }
  // convert timeRanges.
  media::TimeIntervals ranges;
  for (size_t i = 0; i < timeRanges.Length(); i++) {
    ranges +=
      media::TimeInterval(media::TimeUnit::FromMicroseconds(timeRanges[i].start),
                          media::TimeUnit::FromMicroseconds(timeRanges[i].end));
  }
  return ranges;
}

void
MP4TrackDemuxer::NotifyDataArrived()
{
  AutoPinned<MediaResource> resource(mParent->mResource);
  nsTArray<MediaByteRange> byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv)) {
    return;
  }
  MonitorAutoLock mon(mMonitor);
  mIndex->UpdateMoofIndex(byteRanges);
}

void
MP4TrackDemuxer::BreakCycles()
{
  mParent = nullptr;
}

} // namespace mozilla
