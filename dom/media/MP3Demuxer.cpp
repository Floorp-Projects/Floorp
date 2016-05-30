/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Demuxer.h"

#include <inttypes.h>
#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"
#include "VideoUtils.h"
#include "TimeUnits.h"
#include "prenv.h"

#ifdef PR_LOGGING
mozilla::LazyLogModule gMP3DemuxerLog("MP3Demuxer");
#define MP3LOG(msg, ...) \
  MOZ_LOG(gMP3DemuxerLog, LogLevel::Debug, ("MP3Demuxer " msg, ##__VA_ARGS__))
#define MP3LOGV(msg, ...) \
  MOZ_LOG(gMP3DemuxerLog, LogLevel::Verbose, ("MP3Demuxer " msg, ##__VA_ARGS__))
#else
#define MP3LOG(msg, ...)
#define MP3LOGV(msg, ...)
#endif

using mozilla::media::TimeUnit;
using mozilla::media::TimeInterval;
using mozilla::media::TimeIntervals;
using mp4_demuxer::ByteReader;

namespace mozilla {
namespace mp3 {

// MP3Demuxer

MP3Demuxer::MP3Demuxer(MediaResource* aSource)
  : mSource(aSource)
{}

bool
MP3Demuxer::InitInternal() {
  if (!mTrackDemuxer) {
    mTrackDemuxer = new MP3TrackDemuxer(mSource);
  }
  return mTrackDemuxer->Init();
}

RefPtr<MP3Demuxer::InitPromise>
MP3Demuxer::Init() {
  if (!InitInternal()) {
    MP3LOG("MP3Demuxer::Init() failure: waiting for data");

    return InitPromise::CreateAndReject(
      DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  MP3LOG("MP3Demuxer::Init() successful");
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

bool
MP3Demuxer::HasTrackType(TrackInfo::TrackType aType) const {
  return aType == TrackInfo::kAudioTrack;
}

uint32_t
MP3Demuxer::GetNumberTracks(TrackInfo::TrackType aType) const {
  return aType == TrackInfo::kAudioTrack ? 1u : 0u;
}

already_AddRefed<MediaTrackDemuxer>
MP3Demuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber) {
  if (!mTrackDemuxer) {
    return nullptr;
  }
  return RefPtr<MP3TrackDemuxer>(mTrackDemuxer).forget();
}

bool
MP3Demuxer::IsSeekable() const {
  return true;
}

void
MP3Demuxer::NotifyDataArrived() {
  // TODO: bug 1169485.
  NS_WARNING("Unimplemented function NotifyDataArrived");
  MP3LOGV("NotifyDataArrived()");
}

void
MP3Demuxer::NotifyDataRemoved() {
  // TODO: bug 1169485.
  NS_WARNING("Unimplemented function NotifyDataRemoved");
  MP3LOGV("NotifyDataRemoved()");
}


// MP3TrackDemuxer

MP3TrackDemuxer::MP3TrackDemuxer(MediaResource* aSource)
  : mSource(aSource)
  , mOffset(0)
  , mFirstFrameOffset(0)
  , mNumParsedFrames(0)
  , mFrameIndex(0)
  , mTotalFrameLen(0)
  , mSamplesPerFrame(0)
  , mSamplesPerSecond(0)
  , mChannels(0)
{
  Reset();
}

bool
MP3TrackDemuxer::Init() {
  Reset();
  FastSeek(TimeUnit());
  // Read the first frame to fetch sample rate and other meta data.
  RefPtr<MediaRawData> frame(GetNextFrame(FindFirstFrame()));

  MP3LOG("Init StreamLength()=%" PRId64 " first-frame-found=%d",
         StreamLength(), !!frame);

  if (!frame) {
    return false;
  }

  // Rewind back to the stream begin to avoid dropping the first frame.
  FastSeek(TimeUnit());

  if (!mInfo) {
    mInfo = MakeUnique<AudioInfo>();
  }

  mInfo->mRate = mSamplesPerSecond;
  mInfo->mChannels = mChannels;
  mInfo->mBitDepth = 16;
  mInfo->mMimeType = "audio/mpeg";
  mInfo->mDuration = Duration().ToMicroseconds();

  MP3LOG("Init mInfo={mRate=%d mChannels=%d mBitDepth=%d mDuration=%" PRId64 "}",
         mInfo->mRate, mInfo->mChannels, mInfo->mBitDepth,
         mInfo->mDuration);

  return mSamplesPerSecond && mChannels;
}

media::TimeUnit
MP3TrackDemuxer::SeekPosition() const {
  TimeUnit pos = Duration(mFrameIndex);
  if (Duration() > TimeUnit()) {
    pos = std::min(Duration(), pos);
  }
  return pos;
}

const FrameParser::Frame&
MP3TrackDemuxer::LastFrame() const {
  return mParser.PrevFrame();
}

RefPtr<MediaRawData>
MP3TrackDemuxer::DemuxSample() {
  return GetNextFrame(FindNextFrame());
}

const ID3Parser::ID3Header&
MP3TrackDemuxer::ID3Header() const {
  return mParser.ID3Header();
}

const FrameParser::VBRHeader&
MP3TrackDemuxer::VBRInfo() const {
  return mParser.VBRInfo();
}

UniquePtr<TrackInfo>
MP3TrackDemuxer::GetInfo() const {
  return mInfo->Clone();
}

RefPtr<MP3TrackDemuxer::SeekPromise>
MP3TrackDemuxer::Seek(TimeUnit aTime) {
  // Efficiently seek to the position.
  FastSeek(aTime);
  // Correct seek position by scanning the next frames.
  const TimeUnit seekTime = ScanUntil(aTime);

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

TimeUnit
MP3TrackDemuxer::FastSeek(const TimeUnit& aTime) {
  MP3LOG("FastSeek(%" PRId64 ") avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         aTime.ToMicroseconds(), AverageFrameLength(), mNumParsedFrames,
         mFrameIndex, mOffset);

  const auto& vbr = mParser.VBRInfo();
  if (!aTime.ToMicroseconds()) {
    // Quick seek to the beginning of the stream.
    mFrameIndex = 0;
  } else if (vbr.IsTOCPresent() && Duration().ToMicroseconds() > 0) {
    // Use TOC for more precise seeking.
    const float durationFrac = static_cast<float>(aTime.ToMicroseconds()) /
                                                  Duration().ToMicroseconds();
    mFrameIndex = FrameIndexFromOffset(vbr.Offset(durationFrac));
  } else if (AverageFrameLength() > 0) {
    mFrameIndex = FrameIndexFromTime(aTime);
  }

  mOffset = OffsetFromFrameIndex(mFrameIndex);

  if (mOffset > mFirstFrameOffset && StreamLength() > 0) {
    mOffset = std::min(StreamLength() - 1, mOffset);
  }

  mParser.EndFrameSession();

  MP3LOG("FastSeek End TOC=%d avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mFirstFrameOffset=%llu mOffset=%" PRIu64
         " SL=%llu NumBytes=%u",
         vbr.IsTOCPresent(), AverageFrameLength(), mNumParsedFrames, mFrameIndex,
         mFirstFrameOffset, mOffset, StreamLength(), vbr.NumBytes().valueOr(0));

  return Duration(mFrameIndex);
}

TimeUnit
MP3TrackDemuxer::ScanUntil(const TimeUnit& aTime) {
  MP3LOG("ScanUntil(%" PRId64 ") avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         aTime.ToMicroseconds(), AverageFrameLength(), mNumParsedFrames,
         mFrameIndex, mOffset);

  if (!aTime.ToMicroseconds()) {
    return FastSeek(aTime);
  }

  if (Duration(mFrameIndex) > aTime) {
    FastSeek(aTime);
  }

  if (Duration(mFrameIndex + 1) > aTime) {
    return SeekPosition();
  }

  MediaByteRange nextRange = FindNextFrame();
  while (SkipNextFrame(nextRange) && Duration(mFrameIndex + 1) < aTime) {
    nextRange = FindNextFrame();
    MP3LOGV("ScanUntil* avgFrameLen=%f mNumParsedFrames=%" PRIu64
            " mFrameIndex=%" PRId64 " mOffset=%" PRIu64 " Duration=%" PRId64,
            AverageFrameLength(), mNumParsedFrames,
            mFrameIndex, mOffset, Duration(mFrameIndex + 1).ToMicroseconds());
  }

  MP3LOG("ScanUntil End avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         AverageFrameLength(), mNumParsedFrames, mFrameIndex, mOffset);

  return SeekPosition();
}

RefPtr<MP3TrackDemuxer::SamplesPromise>
MP3TrackDemuxer::GetSamples(int32_t aNumSamples) {
  MP3LOGV("GetSamples(%d) Begin mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64 " mSamplesPerFrame=%d "
          "mSamplesPerSecond=%d mChannels=%d",
          aNumSamples, mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(
        DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  RefPtr<SamplesHolder> frames = new SamplesHolder();

  while (aNumSamples--) {
    RefPtr<MediaRawData> frame(GetNextFrame(FindNextFrame()));
    if (!frame) {
      break;
    }

    frames->mSamples.AppendElement(frame);
  }

  MP3LOGV("GetSamples() End mSamples.Size()=%d aNumSamples=%d mOffset=%" PRIu64
          " mNumParsedFrames=%" PRIu64 " mFrameIndex=%" PRId64
          " mTotalFrameLen=%" PRIu64 " mSamplesPerFrame=%d mSamplesPerSecond=%d "
          "mChannels=%d",
          frames->mSamples.Length(), aNumSamples, mOffset, mNumParsedFrames,
          mFrameIndex, mTotalFrameLen, mSamplesPerFrame, mSamplesPerSecond,
          mChannels);

  if (frames->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(
        DemuxerFailureReason::END_OF_STREAM, __func__);
  }
  return SamplesPromise::CreateAndResolve(frames, __func__);
}

void
MP3TrackDemuxer::Reset() {
  MP3LOG("Reset()");

  FastSeek(TimeUnit());
  mParser.Reset();
}

RefPtr<MP3TrackDemuxer::SkipAccessPointPromise>
MP3TrackDemuxer::SkipToNextRandomAccessPoint(TimeUnit aTimeThreshold) {
  // Will not be called for audio-only resources.
  return SkipAccessPointPromise::CreateAndReject(
    SkipFailureHolder(DemuxerFailureReason::DEMUXER_ERROR, 0), __func__);
}

int64_t
MP3TrackDemuxer::GetResourceOffset() const {
  return mOffset;
}

TimeIntervals
MP3TrackDemuxer::GetBuffered() {
  AutoPinned<MediaResource> stream(mSource.GetResource());
  TimeIntervals buffered;

  if (Duration() > TimeUnit() && stream->IsDataCachedToEndOfResource(0)) {
    // Special case completely cached files. This also handles local files.
    buffered += TimeInterval(TimeUnit(), Duration());
    MP3LOGV("buffered = [[%" PRId64 ", %" PRId64 "]]",
            TimeUnit().ToMicroseconds(), Duration().ToMicroseconds());
    return buffered;
  }

  MediaByteRangeSet ranges;
  nsresult rv = stream->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(rv, buffered);

  for (const auto& range: ranges) {
    if (range.IsEmpty()) {
      continue;
    }
    TimeUnit start = Duration(FrameIndexFromOffset(range.mStart));
    TimeUnit end = Duration(FrameIndexFromOffset(range.mEnd));
    MP3LOGV("buffered += [%" PRId64 ", %" PRId64 "]",
            start.ToMicroseconds(), end.ToMicroseconds());
    buffered += TimeInterval(start, end);
  }

  return buffered;
}

int64_t
MP3TrackDemuxer::StreamLength() const {
  return mSource.GetLength();
}

TimeUnit
MP3TrackDemuxer::Duration() const {
  if (!mNumParsedFrames) {
    return TimeUnit::FromMicroseconds(-1);
  }

  int64_t numFrames = 0;
  const auto numAudioFrames = mParser.VBRInfo().NumAudioFrames();
  if (mParser.VBRInfo().IsValid()) {
    // VBR headers don't include the VBR header frame.
    numFrames = numAudioFrames.value() + 1;
  } else {
    const int64_t streamLen = StreamLength();
    if (streamLen < 0) {
      // Unknown length, we can't estimate duration.
      return TimeUnit::FromMicroseconds(-1);
    }
    if (AverageFrameLength() > 0) {
      numFrames = (streamLen - mFirstFrameOffset) / AverageFrameLength();
    }
  }
  return Duration(numFrames);
}

TimeUnit
MP3TrackDemuxer::Duration(int64_t aNumFrames) const {
  if (!mSamplesPerSecond) {
    return TimeUnit::FromMicroseconds(-1);
  }

  const double usPerFrame = USECS_PER_S * mSamplesPerFrame / mSamplesPerSecond;
  return TimeUnit::FromMicroseconds(aNumFrames * usPerFrame);
}

MediaByteRange
MP3TrackDemuxer::FindFirstFrame() {
  static const int MIN_SUCCESSIVE_FRAMES = 4;

  MediaByteRange candidateFrame = FindNextFrame();
  int numSuccFrames = candidateFrame.Length() > 0;
  MediaByteRange currentFrame = candidateFrame;
  MP3LOGV("FindFirst() first candidate frame: mOffset=%" PRIu64 " Length()=%" PRIu64,
          candidateFrame.mStart, candidateFrame.Length());

  while (candidateFrame.Length() && numSuccFrames < MIN_SUCCESSIVE_FRAMES) {
    mParser.EndFrameSession();
    mOffset = currentFrame.mEnd;
    const MediaByteRange prevFrame = currentFrame;

    // FindNextFrame() here will only return frames consistent with our candidate frame.
    currentFrame = FindNextFrame();
    numSuccFrames += currentFrame.Length() > 0;
    // Multiple successive false positives, which wouldn't be caught by the consistency
    // checks alone, can be detected by wrong alignment (non-zero gap between frames).
    const int64_t frameSeparation = currentFrame.mStart - prevFrame.mEnd;

    if (!currentFrame.Length() || frameSeparation != 0) {
      MP3LOGV("FindFirst() not enough successive frames detected, "
              "rejecting candidate frame: successiveFrames=%d, last Length()=%" PRIu64
              ", last frameSeparation=%" PRId64, numSuccFrames, currentFrame.Length(),
              frameSeparation);

      mParser.ResetFrameData();
      mOffset = candidateFrame.mStart + 1;
      candidateFrame = FindNextFrame();
      numSuccFrames = candidateFrame.Length() > 0;
      currentFrame = candidateFrame;
      MP3LOGV("FindFirst() new candidate frame: mOffset=%" PRIu64 " Length()=%" PRIu64,
              candidateFrame.mStart, candidateFrame.Length());
    }
  }

  if (numSuccFrames >= MIN_SUCCESSIVE_FRAMES) {
    MP3LOG("FindFirst() accepting candidate frame: "
            "successiveFrames=%d", numSuccFrames);
  } else {
    MP3LOG("FindFirst() no suitable first frame found");
  }
  return candidateFrame;
}

static bool
VerifyFrameConsistency(
    const FrameParser::Frame& aFrame1, const FrameParser::Frame& aFrame2) {
  const auto& h1 = aFrame1.Header();
  const auto& h2 = aFrame2.Header();

  return h1.IsValid() && h2.IsValid() &&
         h1.Layer() == h2.Layer() &&
         h1.SlotSize() == h2.SlotSize() &&
         h1.SamplesPerFrame() == h2.SamplesPerFrame() &&
         h1.Channels() == h2.Channels() &&
         h1.SampleRate() == h2.SampleRate() &&
         h1.RawVersion() == h2.RawVersion() &&
         h1.RawProtection() == h2.RawProtection();
}

MediaByteRange
MP3TrackDemuxer::FindNextFrame() {
  static const int BUFFER_SIZE = 64;
  static const int MAX_SKIPPED_BYTES = 1024 * BUFFER_SIZE;

  MP3LOGV("FindNext() Begin mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d",
          mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  uint8_t buffer[BUFFER_SIZE];
  int32_t read = 0;

  bool foundFrame = false;
  int64_t frameHeaderOffset = 0;

  // Check whether we've found a valid MPEG frame.
  while (!foundFrame) {
    if ((!mParser.FirstFrame().Length() &&
         mOffset - mParser.ID3Header().Size() > MAX_SKIPPED_BYTES) ||
        (read = Read(buffer, mOffset, BUFFER_SIZE)) == 0) {
      MP3LOG("FindNext() EOS or exceeded MAX_SKIPPED_BYTES without a frame");
      // This is not a valid MPEG audio stream or we've reached EOS, give up.
      break;
    }

    ByteReader reader(buffer, read);
    uint32_t bytesToSkip = 0;
    foundFrame = mParser.Parse(&reader, &bytesToSkip);
    frameHeaderOffset = mOffset + reader.Offset() - FrameParser::FrameHeader::SIZE;

    // If we've found neither an MPEG frame header nor an ID3v2 tag,
    // the reader shouldn't have any bytes remaining.
    MOZ_ASSERT(foundFrame || bytesToSkip || !reader.Remaining());
    reader.DiscardRemaining();

    if (foundFrame && mParser.FirstFrame().Length() &&
        !VerifyFrameConsistency(mParser.FirstFrame(), mParser.CurrentFrame())) {
      // We've likely hit a false-positive, ignore it and proceed with the
      // search for the next valid frame.
      foundFrame = false;
      mOffset = frameHeaderOffset + 1;
      mParser.EndFrameSession();
    } else {
      // Advance mOffset by the amount of bytes read and if necessary,
      // skip an ID3v2 tag which stretches beyond the current buffer.
      NS_ENSURE_TRUE(mOffset + read + bytesToSkip > mOffset,
                     MediaByteRange(0, 0));
      mOffset += read + bytesToSkip;
    }
  }

  if (!foundFrame || !mParser.CurrentFrame().Length()) {
    MP3LOG("FindNext() Exit foundFrame=%d mParser.CurrentFrame().Length()=%d ",
           foundFrame, mParser.CurrentFrame().Length());
    return { 0, 0 };
  }

  MP3LOGV("FindNext() End mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " frameHeaderOffset=%d"
          " mTotalFrameLen=%" PRIu64 " mSamplesPerFrame=%d mSamplesPerSecond=%d"
          " mChannels=%d",
          mOffset, mNumParsedFrames, mFrameIndex, frameHeaderOffset,
          mTotalFrameLen, mSamplesPerFrame, mSamplesPerSecond, mChannels);

  return { frameHeaderOffset, frameHeaderOffset + mParser.CurrentFrame().Length() };
}

bool
MP3TrackDemuxer::SkipNextFrame(const MediaByteRange& aRange) {
  if (!mNumParsedFrames || !aRange.Length()) {
    // We can't skip the first frame, since it could contain VBR headers.
    RefPtr<MediaRawData> frame(GetNextFrame(aRange));
    return frame;
  }

  UpdateState(aRange);

  MP3LOGV("SkipNext() End mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d",
          mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  return true;
}

already_AddRefed<MediaRawData>
MP3TrackDemuxer::GetNextFrame(const MediaByteRange& aRange) {
  MP3LOG("GetNext() Begin({mStart=%" PRId64 " Length()=%" PRId64 "})",
         aRange.mStart, aRange.Length());
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> frame = new MediaRawData();
  frame->mOffset = aRange.mStart;

  nsAutoPtr<MediaRawDataWriter> frameWriter(frame->CreateWriter());
  if (!frameWriter->SetSize(aRange.Length())) {
    MP3LOG("GetNext() Exit failed to allocated media buffer");
    return nullptr;
  }

  const uint32_t read = Read(frameWriter->Data(), frame->mOffset, frame->Size());

  if (read != aRange.Length()) {
    MP3LOG("GetNext() Exit read=%u frame->Size()=%u", read, frame->Size());
    return nullptr;
  }

  UpdateState(aRange);

  frame->mTime = Duration(mFrameIndex - 1).ToMicroseconds();
  frame->mDuration = Duration(1).ToMicroseconds();
  frame->mTimecode = frame->mTime;
  frame->mKeyframe = true;

  MOZ_ASSERT(frame->mTime >= 0);
  MOZ_ASSERT(frame->mDuration > 0);

  if (mNumParsedFrames == 1) {
    // First frame parsed, let's read VBR info if available.
    // TODO: read info that helps with seeking (bug 1163667).
    ByteReader reader(frame->Data(), frame->Size());
    mParser.ParseVBRHeader(&reader);
    reader.DiscardRemaining();
    mFirstFrameOffset = frame->mOffset;
  }

  MP3LOGV("GetNext() End mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d",
          mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  return frame.forget();
}

int64_t
MP3TrackDemuxer::OffsetFromFrameIndex(int64_t aFrameIndex) const {
  int64_t offset = 0;
  const auto& vbr = mParser.VBRInfo();

  if (vbr.IsValid()) {
    offset = mFirstFrameOffset + aFrameIndex * vbr.NumBytes().value() /
             vbr.NumAudioFrames().value();
  } else if (AverageFrameLength() > 0) {
    offset = mFirstFrameOffset + aFrameIndex * AverageFrameLength();
  }

  MP3LOGV("OffsetFromFrameIndex(%" PRId64 ") -> %" PRId64, aFrameIndex, offset);
  return std::max<int64_t>(mFirstFrameOffset, offset);
}

int64_t
MP3TrackDemuxer::FrameIndexFromOffset(int64_t aOffset) const {
  int64_t frameIndex = 0;
  const auto& vbr = mParser.VBRInfo();

  if (vbr.IsValid()) {
    frameIndex = static_cast<float>(aOffset - mFirstFrameOffset) /
                 vbr.NumBytes().value() * vbr.NumAudioFrames().value();
    frameIndex = std::min<int64_t>(vbr.NumAudioFrames().value(), frameIndex);
  } else if (AverageFrameLength() > 0) {
    frameIndex = (aOffset - mFirstFrameOffset) / AverageFrameLength();
  }

  MP3LOGV("FrameIndexFromOffset(%" PRId64 ") -> %" PRId64, aOffset, frameIndex);
  return std::max<int64_t>(0, frameIndex);
}

int64_t
MP3TrackDemuxer::FrameIndexFromTime(const media::TimeUnit& aTime) const {
  int64_t frameIndex = 0;
  if (mSamplesPerSecond > 0 && mSamplesPerFrame > 0) {
    frameIndex = aTime.ToSeconds() * mSamplesPerSecond / mSamplesPerFrame - 1;
  }

  MP3LOGV("FrameIndexFromOffset(%fs) -> %" PRId64, aTime.ToSeconds(), frameIndex);
  return std::max<int64_t>(0, frameIndex);
}

void
MP3TrackDemuxer::UpdateState(const MediaByteRange& aRange) {
  // Prevent overflow.
  if (mTotalFrameLen + aRange.Length() < mTotalFrameLen) {
    // These variables have a linear dependency and are only used to derive the
    // average frame length.
    mTotalFrameLen /= 2;
    mNumParsedFrames /= 2;
  }

  // Full frame parsed, move offset to its end.
  mOffset = aRange.mEnd;

  mTotalFrameLen += aRange.Length();

  if (!mSamplesPerFrame) {
    mSamplesPerFrame = mParser.CurrentFrame().Header().SamplesPerFrame();
    mSamplesPerSecond = mParser.CurrentFrame().Header().SampleRate();
    mChannels = mParser.CurrentFrame().Header().Channels();
  }

  ++mNumParsedFrames;
  ++mFrameIndex;
  MOZ_ASSERT(mFrameIndex > 0);

  // Prepare the parser for the next frame parsing session.
  mParser.EndFrameSession();
}

int32_t
MP3TrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize) {
  MP3LOGV("MP3TrackDemuxer::Read(%p %" PRId64 " %d)", aBuffer, aOffset, aSize);

  const int64_t streamLen = StreamLength();
  if (mInfo && streamLen > 0) {
    // Prevent blocking reads after successful initialization.
    aSize = std::min<int64_t>(aSize, streamLen - aOffset);
  }

  uint32_t read = 0;
  MP3LOGV("MP3TrackDemuxer::Read        -> ReadAt(%d)", aSize);
  const nsresult rv = mSource.ReadAt(aOffset, reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize), &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return static_cast<int32_t>(read);
}

double
MP3TrackDemuxer::AverageFrameLength() const {
  if (mNumParsedFrames) {
    return static_cast<double>(mTotalFrameLen) / mNumParsedFrames;
  }
  const auto& vbr = mParser.VBRInfo();
  if (vbr.IsValid() && vbr.NumAudioFrames().value() + 1) {
    return static_cast<double>(vbr.NumBytes().value()) /
           (vbr.NumAudioFrames().value() + 1);
  }
  return 0.0;
}

// FrameParser

namespace frame_header {
// FrameHeader mRaw byte offsets.
static const int SYNC1 = 0;
static const int SYNC2_VERSION_LAYER_PROTECTION = 1;
static const int BITRATE_SAMPLERATE_PADDING_PRIVATE = 2;
static const int CHANNELMODE_MODEEXT_COPY_ORIG_EMPH = 3;
} // namespace frame_header

FrameParser::FrameParser()
{
}

void
FrameParser::Reset() {
  mID3Parser.Reset();
  mFrame.Reset();
}

void
FrameParser::ResetFrameData() {
  mFrame.Reset();
  mFirstFrame.Reset();
  mPrevFrame.Reset();
}

void
FrameParser::EndFrameSession() {
  if (!mID3Parser.Header().IsValid()) {
    // Reset ID3 tags only if we have not parsed a valid ID3 header yet.
    mID3Parser.Reset();
  }
  mPrevFrame = mFrame;
  mFrame.Reset();
}

const FrameParser::Frame&
FrameParser::CurrentFrame() const {
  return mFrame;
}

const FrameParser::Frame&
FrameParser::PrevFrame() const {
  return mPrevFrame;
}

const FrameParser::Frame&
FrameParser::FirstFrame() const {
  return mFirstFrame;
}

const ID3Parser::ID3Header&
FrameParser::ID3Header() const {
  return mID3Parser.Header();
}

const FrameParser::VBRHeader&
FrameParser::VBRInfo() const {
  return mVBRHeader;
}

bool
FrameParser::Parse(ByteReader* aReader, uint32_t* aBytesToSkip) {
  MOZ_ASSERT(aReader && aBytesToSkip);
  *aBytesToSkip = 0;

  if (!mID3Parser.Header().Size() && !mFirstFrame.Length()) {
    // No MP3 frames have been parsed yet, look for ID3v2 headers at file begin.
    // ID3v1 tags may only be at file end.
    // TODO: should we try to read ID3 tags at end of file/mid-stream, too?
    const size_t prevReaderOffset = aReader->Offset();
    const uint32_t tagSize = mID3Parser.Parse(aReader);
    if (tagSize) {
      // ID3 tag found, skip past it.
      const uint32_t skipSize = tagSize - ID3Parser::ID3Header::SIZE;

      if (skipSize > aReader->Remaining()) {
        // Skipping across the ID3v2 tag would take us past the end of the buffer, therefore we
        // return immediately and let the calling function handle skipping the rest of the tag.
        MP3LOGV("ID3v2 tag detected, size=%d,"
                " needing to skip %d bytes past the current buffer",
                tagSize, skipSize - aReader->Remaining());
        *aBytesToSkip = skipSize - aReader->Remaining();
        return false;
      }
      MP3LOGV("ID3v2 tag detected, size=%d", tagSize);
      aReader->Read(skipSize);
    } else {
      // No ID3v2 tag found, rewinding reader in order to search for a MPEG frame header.
      aReader->Seek(prevReaderOffset);
    }
  }

  while (aReader->CanRead8() && !mFrame.ParseNext(aReader->ReadU8())) { }

  if (mFrame.Length()) {
    // MP3 frame found.
    if (!mFirstFrame.Length()) {
      mFirstFrame = mFrame;
    }
    // Indicate success.
    return true;
  }
  return false;
}

// FrameParser::Header

FrameParser::FrameHeader::FrameHeader()
{
  Reset();
}

uint8_t
FrameParser::FrameHeader::Sync1() const {
  return mRaw[frame_header::SYNC1];
}

uint8_t
FrameParser::FrameHeader::Sync2() const {
  return 0x7 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 5;
}

uint8_t
FrameParser::FrameHeader::RawVersion() const {
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 3;
}

uint8_t
FrameParser::FrameHeader::RawLayer() const {
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 1;
}

uint8_t
FrameParser::FrameHeader::RawProtection() const {
  return 0x1 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 6;
}

uint8_t
FrameParser::FrameHeader::RawBitrate() const {
  return 0xF & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 4;
}

uint8_t
FrameParser::FrameHeader::RawSampleRate() const {
  return 0x3 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 2;
}

uint8_t
FrameParser::FrameHeader::Padding() const {
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 1;
}

uint8_t
FrameParser::FrameHeader::Private() const {
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE];
}

uint8_t
FrameParser::FrameHeader::RawChannelMode() const {
  return 0x3 & mRaw[frame_header::CHANNELMODE_MODEEXT_COPY_ORIG_EMPH] >> 6;
}

int32_t
FrameParser::FrameHeader::Layer() const {
  static const uint8_t LAYERS[4] = { 0, 3, 2, 1 };

  return LAYERS[RawLayer()];
}

int32_t
FrameParser::FrameHeader::SampleRate() const {
  // Sample rates - use [version][srate]
  static const uint16_t SAMPLE_RATE[4][4] = {
    { 11025, 12000,  8000, 0 }, // MPEG 2.5
    {     0,     0,     0, 0 }, // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 }  // MPEG 1
  };

  return SAMPLE_RATE[RawVersion()][RawSampleRate()];
}

int32_t
FrameParser::FrameHeader::Channels() const {
  // 3 is single channel (mono), any other value is some variant of dual
  // channel.
  return RawChannelMode() == 3 ? 1 : 2;
}

int32_t
FrameParser::FrameHeader::SamplesPerFrame() const {
  // Samples per frame - use [version][layer]
  static const uint16_t FRAME_SAMPLE[4][4] = {
    // Layer     3     2     1       Version
    {      0,  576, 1152,  384 }, // 2.5
    {      0,    0,    0,    0 }, // Reserved
    {      0,  576, 1152,  384 }, // 2
    {      0, 1152, 1152,  384 }  // 1
  };

  return FRAME_SAMPLE[RawVersion()][RawLayer()];
}

int32_t
FrameParser::FrameHeader::Bitrate() const {
  // Bitrates - use [version][layer][bitrate]
  static const uint16_t BITRATE[4][4][16] = {
    { // Version 2.5
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
    },
    { // Reserved
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }  // Invalid
    },
    { // Version 2
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
    },
    { // Version 1
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,  32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }, // Layer 3
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer 2
      { 0,  32,  64,  96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // Layer 1
    }
  };

  return 1000 * BITRATE[RawVersion()][RawLayer()][RawBitrate()];
}

int32_t
FrameParser::FrameHeader::SlotSize() const {
  // Slot size (MPEG unit of measurement) - use [layer]
  static const uint8_t SLOT_SIZE[4] = { 0, 1, 1, 4 }; // Rsvd, 3, 2, 1

  return SLOT_SIZE[RawLayer()];
}

bool
FrameParser::FrameHeader::ParseNext(uint8_t c) {
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool
FrameParser::FrameHeader::IsValid(int aPos) const {
  if (aPos >= SIZE) {
    return true;
  }
  if (aPos == frame_header::SYNC1) {
    return Sync1() == 0xFF;
  }
  if (aPos == frame_header::SYNC2_VERSION_LAYER_PROTECTION) {
    return Sync2() == 7 &&
           RawVersion() != 1 &&
           Layer() == 3;
  }
  if (aPos == frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE) {
    return RawBitrate() != 0xF && RawBitrate() != 0 &&
           RawSampleRate() != 3;
  }
  return true;
}

bool
FrameParser::FrameHeader::IsValid() const {
  return mPos >= SIZE;
}

void
FrameParser::FrameHeader::Reset() {
  mPos = 0;
}

bool
FrameParser::FrameHeader::Update(uint8_t c) {
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

// FrameParser::VBRHeader

namespace vbr_header {
static const char* TYPE_STR[3] = {"NONE", "XING", "VBRI"};
static const uint32_t TOC_SIZE = 100;
} // namespace vbr_header

FrameParser::VBRHeader::VBRHeader()
  : mType(NONE)
{
}

FrameParser::VBRHeader::VBRHeaderType
FrameParser::VBRHeader::Type() const {
  return mType;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::NumAudioFrames() const {
  return mNumAudioFrames;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::NumBytes() const {
  return mNumBytes;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::Scale() const {
  return mScale;
}

bool
FrameParser::VBRHeader::IsTOCPresent() const {
  return mTOC.size() == vbr_header::TOC_SIZE;
}

bool
FrameParser::VBRHeader::IsValid() const {
  return mType != NONE &&
         mNumAudioFrames.valueOr(0) > 0 &&
         mNumBytes.valueOr(0) > 0 &&
         // We don't care about the scale for any computations here.
         // mScale < 101 &&
         true;
}

int64_t
FrameParser::VBRHeader::Offset(float aDurationFac) const {
  if (!IsTOCPresent()) {
    return -1;
  }

  // Constrain the duration percentage to [0, 99].
  const float durationPer = 100.0f * std::min(0.99f, std::max(0.0f, aDurationFac));
  const size_t fullPer = durationPer;
  const float rest = durationPer - fullPer;

  MOZ_ASSERT(fullPer < mTOC.size());
  int64_t offset = mTOC.at(fullPer);

  if (rest > 0.0 && fullPer + 1 < mTOC.size()) {
    offset += rest * (mTOC.at(fullPer + 1) - offset);
  }

  return offset;
}

bool
FrameParser::VBRHeader::ParseXing(ByteReader* aReader) {
  static const uint32_t XING_TAG = BigEndian::readUint32("Xing");
  static const uint32_t INFO_TAG = BigEndian::readUint32("Info");

  enum Flags {
    NUM_FRAMES = 0x01,
    NUM_BYTES = 0x02,
    TOC = 0x04,
    VBR_SCALE = 0x08
  };

  MOZ_ASSERT(aReader);
  const size_t prevReaderOffset = aReader->Offset();

  // We have to search for the Xing header as its position can change.
  while (aReader->CanRead32() &&
         aReader->PeekU32() != XING_TAG && aReader->PeekU32() != INFO_TAG) {
    aReader->Read(1);
  }

  if (aReader->CanRead32()) {
    // Skip across the VBR header ID tag.
    aReader->ReadU32();
    mType = XING;
  }
  uint32_t flags = 0;
  if (aReader->CanRead32()) {
    flags = aReader->ReadU32();
  }
  if (flags & NUM_FRAMES && aReader->CanRead32()) {
    mNumAudioFrames = Some(aReader->ReadU32());
  }
  if (flags & NUM_BYTES && aReader->CanRead32()) {
    mNumBytes = Some(aReader->ReadU32());
  }
  if (flags & TOC && aReader->Remaining() >= vbr_header::TOC_SIZE) {
    if (!mNumBytes) {
      // We don't have the stream size to calculate offsets, skip the TOC.
      aReader->Read(vbr_header::TOC_SIZE);
    } else {
      mTOC.clear();
      mTOC.reserve(vbr_header::TOC_SIZE);
      for (size_t i = 0; i < vbr_header::TOC_SIZE; ++i) {
        mTOC.push_back(1.0f / 256.0f * aReader->ReadU8() * mNumBytes.value());
      }
    }
  }
  if (flags & VBR_SCALE && aReader->CanRead32()) {
    mScale = Some(aReader->ReadU32());
  }

  aReader->Seek(prevReaderOffset);
  return mType == XING;
}

bool
FrameParser::VBRHeader::ParseVBRI(ByteReader* aReader) {
  static const uint32_t TAG = BigEndian::readUint32("VBRI");
  static const uint32_t OFFSET = 32 + FrameParser::FrameHeader::SIZE;
  static const uint32_t FRAME_COUNT_OFFSET = OFFSET + 14;
  static const uint32_t MIN_FRAME_SIZE = OFFSET + 26;

  MOZ_ASSERT(aReader);
  // ParseVBRI assumes that the ByteReader offset points to the beginning of a frame,
  // therefore as a simple check, we look for the presence of a frame sync at that position.
  MOZ_ASSERT((aReader->PeekU16() & 0xFFE0) == 0xFFE0);
  const size_t prevReaderOffset = aReader->Offset();

  // VBRI have a fixed relative position, so let's check for it there.
  if (aReader->Remaining() > MIN_FRAME_SIZE) {
    aReader->Seek(prevReaderOffset + OFFSET);
    if (aReader->ReadU32() == TAG) {
      aReader->Seek(prevReaderOffset + FRAME_COUNT_OFFSET);
      mNumAudioFrames = Some(aReader->ReadU32());
      mType = VBRI;
      aReader->Seek(prevReaderOffset);
      return true;
    }
  }
  aReader->Seek(prevReaderOffset);
  return false;
}

bool
FrameParser::VBRHeader::Parse(ByteReader* aReader) {
  const bool rv = ParseVBRI(aReader) || ParseXing(aReader);
  if (rv) {
    MP3LOG("VBRHeader::Parse found valid VBR/CBR header: type=%s"
           " NumAudioFrames=%u NumBytes=%u Scale=%u TOC-size=%u",
           vbr_header::TYPE_STR[Type()], NumAudioFrames().valueOr(0),
           NumBytes().valueOr(0), Scale().valueOr(0), mTOC.size());
  }
  return rv;
}

// FrameParser::Frame

void
FrameParser::Frame::Reset() {
  mHeader.Reset();
}

int32_t
FrameParser::Frame::Length() const {
  if (!mHeader.IsValid() || !mHeader.SampleRate()) {
    return 0;
  }

  const float bitsPerSample = mHeader.SamplesPerFrame() / 8.0f;
  const int32_t frameLen = bitsPerSample * mHeader.Bitrate() /
                           mHeader.SampleRate() +
                           mHeader.Padding() * mHeader.SlotSize();
  return frameLen;
}

bool
FrameParser::Frame::ParseNext(uint8_t c) {
  return mHeader.ParseNext(c);
}

const FrameParser::FrameHeader&
FrameParser::Frame::Header() const {
  return mHeader;
}

bool
FrameParser::ParseVBRHeader(ByteReader* aReader) {
  return mVBRHeader.Parse(aReader);
}

// ID3Parser

// Constants
namespace id3_header {
static const int ID_LEN = 3;
static const int VERSION_LEN = 2;
static const int FLAGS_LEN = 1;
static const int SIZE_LEN = 4;

static const int ID_END = ID_LEN;
static const int VERSION_END = ID_END + VERSION_LEN;
static const int FLAGS_END = VERSION_END + FLAGS_LEN;
static const int SIZE_END = FLAGS_END + SIZE_LEN;

static const uint8_t ID[ID_LEN] = {'I', 'D', '3'};

static const uint8_t MIN_MAJOR_VER = 2;
static const uint8_t MAX_MAJOR_VER = 4;
} // namespace id3_header

uint32_t
ID3Parser::Parse(ByteReader* aReader) {
  MOZ_ASSERT(aReader);

  while (aReader->CanRead8() && !mHeader.ParseNext(aReader->ReadU8())) { }

  if (mHeader.IsValid()) {
    // Header found, return total tag size.
    return ID3Header::SIZE + Header().Size() + Header().FooterSize();
  }
  return 0;
}

void
ID3Parser::Reset() {
  mHeader.Reset();
}

const ID3Parser::ID3Header&
ID3Parser::Header() const {
  return mHeader;
}

// ID3Parser::Header

ID3Parser::ID3Header::ID3Header()
{
  Reset();
}

void
ID3Parser::ID3Header::Reset() {
  mSize = 0;
  mPos = 0;
}

uint8_t
ID3Parser::ID3Header::MajorVersion() const {
  return mRaw[id3_header::ID_END];
}

uint8_t
ID3Parser::ID3Header::MinorVersion() const {
  return mRaw[id3_header::ID_END + 1];
}

uint8_t
ID3Parser::ID3Header::Flags() const {
  return mRaw[id3_header::FLAGS_END - id3_header::FLAGS_LEN];
}

uint32_t
ID3Parser::ID3Header::Size() const {
  if (!IsValid()) {
    return 0;
  }
  return mSize;
}

uint8_t
ID3Parser::ID3Header::FooterSize() const {
  if (Flags() & (1 << 4)) {
    return SIZE;
  }
  return 0;
}

bool
ID3Parser::ID3Header::ParseNext(uint8_t c) {
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool
ID3Parser::ID3Header::IsValid(int aPos) const {
  if (aPos >= SIZE) {
    return true;
  }
  const uint8_t c = mRaw[aPos];
  switch (aPos) {
    case 0: case 1: case 2:
      // Expecting "ID3".
      return id3_header::ID[aPos] == c;
    case 3:
      return MajorVersion() >= id3_header::MIN_MAJOR_VER &&
             MajorVersion() <= id3_header::MAX_MAJOR_VER;
    case 4:
      return MinorVersion() < 0xFF;
    case 5:
      // Validate flags for supported versions, see bug 949036.
      return ((0xFF >> MajorVersion()) & c) == 0;
    case 6: case 7: case 8: case 9:
      return c < 0x80;
  }
  return true;
}

bool
ID3Parser::ID3Header::IsValid() const {
  return mPos >= SIZE;
}

bool
ID3Parser::ID3Header::Update(uint8_t c) {
  if (mPos >= id3_header::SIZE_END - id3_header::SIZE_LEN &&
      mPos < id3_header::SIZE_END) {
    mSize <<= 7;
    mSize |= c;
  }
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

} // namespace mp3
} // namespace mozilla
