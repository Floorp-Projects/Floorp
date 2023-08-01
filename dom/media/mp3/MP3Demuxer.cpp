/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Demuxer.h"

#include <algorithm>
#include <inttypes.h>
#include <limits>

#include "ByteWriter.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"

extern mozilla::LazyLogModule gMediaDemuxerLog;
#define MP3LOG(msg, ...) \
  DDMOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, msg, ##__VA_ARGS__)
#define MP3LOGV(msg, ...) \
  DDMOZ_LOG(gMediaDemuxerLog, LogLevel::Verbose, msg, ##__VA_ARGS__)

using mozilla::media::TimeInterval;
using mozilla::media::TimeIntervals;
using mozilla::media::TimeUnit;

namespace mozilla {

// MP3Demuxer

MP3Demuxer::MP3Demuxer(MediaResource* aSource) : mSource(aSource) {
  DDLINKCHILD("source", aSource);
}

bool MP3Demuxer::InitInternal() {
  if (!mTrackDemuxer) {
    mTrackDemuxer = new MP3TrackDemuxer(mSource);
    DDLINKCHILD("track demuxer", mTrackDemuxer.get());
  }
  return mTrackDemuxer->Init();
}

RefPtr<MP3Demuxer::InitPromise> MP3Demuxer::Init() {
  if (!InitInternal()) {
    MP3LOG("MP3Demuxer::Init() failure: waiting for data");

    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                                        __func__);
  }

  MP3LOG("MP3Demuxer::Init() successful");
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

uint32_t MP3Demuxer::GetNumberTracks(TrackInfo::TrackType aType) const {
  return aType == TrackInfo::kAudioTrack ? 1u : 0u;
}

already_AddRefed<MediaTrackDemuxer> MP3Demuxer::GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) {
  if (!mTrackDemuxer) {
    return nullptr;
  }
  return RefPtr<MP3TrackDemuxer>(mTrackDemuxer).forget();
}

bool MP3Demuxer::IsSeekable() const { return true; }

void MP3Demuxer::NotifyDataArrived() {
  // TODO: bug 1169485.
  NS_WARNING("Unimplemented function NotifyDataArrived");
  MP3LOGV("NotifyDataArrived()");
}

void MP3Demuxer::NotifyDataRemoved() {
  // TODO: bug 1169485.
  NS_WARNING("Unimplemented function NotifyDataRemoved");
  MP3LOGV("NotifyDataRemoved()");
}

// MP3TrackDemuxer

MP3TrackDemuxer::MP3TrackDemuxer(MediaResource* aSource)
    : mSource(aSource),
      mFrameLock(false),
      mOffset(0),
      mFirstFrameOffset(0),
      mNumParsedFrames(0),
      mFrameIndex(0),
      mTotalFrameLen(0),
      mSamplesPerFrame(0),
      mSamplesPerSecond(0),
      mChannels(0) {
  DDLINKCHILD("source", aSource);
  Reset();
}

bool MP3TrackDemuxer::Init() {
  Reset();
  FastSeek(TimeUnit());
  // Read the first frame to fetch sample rate and other meta data.
  RefPtr<MediaRawData> frame(GetNextFrame(FindFirstFrame()));

  MP3LOG("Init StreamLength()=%" PRId64 " first-frame-found=%d", StreamLength(),
         !!frame);

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
  mInfo->mDuration = Duration().valueOr(TimeUnit::FromInfinity());

  MP3LOG("Init mInfo={mRate=%d mChannels=%d mBitDepth=%d mDuration=%s (%lfs)}",
         mInfo->mRate, mInfo->mChannels, mInfo->mBitDepth,
         mInfo->mDuration.ToString().get(), mInfo->mDuration.ToSeconds());

  return mSamplesPerSecond && mChannels;
}

media::TimeUnit MP3TrackDemuxer::SeekPosition() const {
  TimeUnit pos = Duration(mFrameIndex);
  auto duration = Duration();
  if (duration) {
    pos = std::min(*duration, pos);
  }
  return pos;
}

const FrameParser::Frame& MP3TrackDemuxer::LastFrame() const {
  return mParser.PrevFrame();
}

RefPtr<MediaRawData> MP3TrackDemuxer::DemuxSample() {
  return GetNextFrame(FindNextFrame());
}

const ID3Parser::ID3Header& MP3TrackDemuxer::ID3Header() const {
  return mParser.ID3Header();
}

const FrameParser::VBRHeader& MP3TrackDemuxer::VBRInfo() const {
  return mParser.VBRInfo();
}

UniquePtr<TrackInfo> MP3TrackDemuxer::GetInfo() const { return mInfo->Clone(); }

RefPtr<MP3TrackDemuxer::SeekPromise> MP3TrackDemuxer::Seek(
    const TimeUnit& aTime) {
  mRemainingEncoderPadding = AssertedCast<int32_t>(mEncoderPadding);
  // Efficiently seek to the position.
  FastSeek(aTime);
  // Correct seek position by scanning the next frames.
  const TimeUnit seekTime = ScanUntil(aTime);

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

TimeUnit MP3TrackDemuxer::FastSeek(const TimeUnit& aTime) {
  MP3LOG("FastSeek(%" PRId64 ") avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         aTime.ToMicroseconds(), AverageFrameLength(), mNumParsedFrames,
         mFrameIndex, mOffset);

  const auto& vbr = mParser.VBRInfo();
  if (aTime.IsZero()) {
    // Quick seek to the beginning of the stream.
    mFrameIndex = 0;
  } else if (vbr.IsTOCPresent() && Duration() &&
             *Duration() != TimeUnit::Zero()) {
    // Use TOC for more precise seeking.
    mFrameIndex = FrameIndexFromOffset(vbr.Offset(aTime, Duration().value()));
  } else if (AverageFrameLength() > 0) {
    mFrameIndex = FrameIndexFromTime(aTime);
  }

  mOffset = OffsetFromFrameIndex(mFrameIndex);

  if (mOffset > mFirstFrameOffset && StreamLength() > 0) {
    mOffset = std::min(StreamLength() - 1, mOffset);
  }

  mParser.EndFrameSession();

  MP3LOG("FastSeek End TOC=%d avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mFirstFrameOffset=%" PRId64
         " mOffset=%" PRIu64 " SL=%" PRId64 " NumBytes=%u",
         vbr.IsTOCPresent(), AverageFrameLength(), mNumParsedFrames,
         mFrameIndex, mFirstFrameOffset, mOffset, StreamLength(),
         vbr.NumBytes().valueOr(0));

  return Duration(mFrameIndex);
}

TimeUnit MP3TrackDemuxer::ScanUntil(const TimeUnit& aTime) {
  MP3LOG("ScanUntil(%" PRId64 ") avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         aTime.ToMicroseconds(), AverageFrameLength(), mNumParsedFrames,
         mFrameIndex, mOffset);

  if (aTime.IsZero()) {
    return FastSeek(aTime);
  }

  if (Duration(mFrameIndex) > aTime) {
    // We've seeked past the target time, rewind back a little to correct it.
    const int64_t rewind = aTime.ToMicroseconds() / 100;
    FastSeek(aTime - TimeUnit::FromMicroseconds(rewind));
  }

  if (Duration(mFrameIndex + 1) > aTime) {
    return SeekPosition();
  }

  MediaByteRange nextRange = FindNextFrame();
  while (SkipNextFrame(nextRange) && Duration(mFrameIndex + 1) < aTime) {
    nextRange = FindNextFrame();
    MP3LOGV("ScanUntil* avgFrameLen=%f mNumParsedFrames=%" PRIu64
            " mFrameIndex=%" PRId64 " mOffset=%" PRIu64 " Duration=%" PRId64,
            AverageFrameLength(), mNumParsedFrames, mFrameIndex, mOffset,
            Duration(mFrameIndex + 1).ToMicroseconds());
  }

  MP3LOG("ScanUntil End avgFrameLen=%f mNumParsedFrames=%" PRIu64
         " mFrameIndex=%" PRId64 " mOffset=%" PRIu64,
         AverageFrameLength(), mNumParsedFrames, mFrameIndex, mOffset);

  return SeekPosition();
}

RefPtr<MP3TrackDemuxer::SamplesPromise> MP3TrackDemuxer::GetSamples(
    int32_t aNumSamples) {
  MP3LOGV("GetSamples(%d) Begin mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d",
          aNumSamples, mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                                           __func__);
  }

  RefPtr<SamplesHolder> frames = new SamplesHolder();

  while (aNumSamples--) {
    RefPtr<MediaRawData> frame(GetNextFrame(FindNextFrame()));
    if (!frame) {
      break;
    }
    if (!frame->HasValidTime()) {
      return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                                             __func__);
    }
    frames->AppendSample(frame);
  }

  MP3LOGV("GetSamples() End mSamples.Size()=%zu aNumSamples=%d mOffset=%" PRIu64
          " mNumParsedFrames=%" PRIu64 " mFrameIndex=%" PRId64
          " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d "
          "mChannels=%d",
          frames->GetSamples().Length(), aNumSamples, mOffset, mNumParsedFrames,
          mFrameIndex, mTotalFrameLen, mSamplesPerFrame, mSamplesPerSecond,
          mChannels);

  if (frames->GetSamples().IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                           __func__);
  }
  return SamplesPromise::CreateAndResolve(frames, __func__);
}

void MP3TrackDemuxer::Reset() {
  MP3LOG("Reset()");

  FastSeek(TimeUnit());
  mParser.Reset();
}

RefPtr<MP3TrackDemuxer::SkipAccessPointPromise>
MP3TrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold) {
  // Will not be called for audio-only resources.
  return SkipAccessPointPromise::CreateAndReject(
      SkipFailureHolder(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, 0), __func__);
}

int64_t MP3TrackDemuxer::GetResourceOffset() const { return mOffset; }

TimeIntervals MP3TrackDemuxer::GetBuffered() {
  AutoPinned<MediaResource> stream(mSource.GetResource());
  TimeIntervals buffered;

  if (Duration() && stream->IsDataCachedToEndOfResource(0)) {
    // Special case completely cached files. This also handles local files.
    buffered += TimeInterval(TimeUnit(), *Duration());
    MP3LOGV("buffered = [[%" PRId64 ", %" PRId64 "]]",
            TimeUnit().ToMicroseconds(), Duration()->ToMicroseconds());
    return buffered;
  }

  MediaByteRangeSet ranges;
  nsresult rv = stream->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(rv, buffered);

  for (const auto& range : ranges) {
    if (range.IsEmpty()) {
      continue;
    }
    TimeUnit start = Duration(FrameIndexFromOffset(range.mStart));
    TimeUnit end = Duration(FrameIndexFromOffset(range.mEnd));
    MP3LOGV("buffered += [%" PRId64 ", %" PRId64 "]", start.ToMicroseconds(),
            end.ToMicroseconds());
    buffered += TimeInterval(start, end);
  }

  // If the number of frames reported by the header is valid,
  // the duration calculated from it is the maximal duration.
  if (ValidNumAudioFrames() && Duration()) {
    TimeInterval duration = TimeInterval(TimeUnit(), *Duration());
    return buffered.Intersection(duration);
  }

  return buffered;
}

int64_t MP3TrackDemuxer::StreamLength() const { return mSource.GetLength(); }

media::NullableTimeUnit NothingIfNegative(TimeUnit aDuration) {
  if (aDuration.IsNegative()) {
    return Nothing();
  }
  return Some(aDuration);
}

media::NullableTimeUnit MP3TrackDemuxer::Duration() const {
  if (!mNumParsedFrames) {
    return Nothing();
  }

  int64_t numFrames = 0;
  const auto numAudioFrames = ValidNumAudioFrames();
  if (numAudioFrames) {
    // VBR headers don't include the VBR header frame.
    numFrames = numAudioFrames.value() + 1;
    return NothingIfNegative(Duration(numFrames) -
                             (EncoderDelay() + Padding()));
  }

  const int64_t streamLen = StreamLength();
  if (streamLen < 0) {  // Live streams.
    // Unknown length, we can't estimate duration.
    return Nothing();
  }
  // We can't early return when streamLen < 0 before checking numAudioFrames
  // since some live radio will give an opening remark before playing music
  // and the duration of the opening talk can be calculated by numAudioFrames.

  int64_t size = streamLen - mFirstFrameOffset;
  MOZ_ASSERT(size);

  if (mParser.ID3v1MetadataFound() && size > 128) {
    size -= 128;
  }

  // If it's CBR, calculate the duration by bitrate.
  if (!mParser.VBRInfo().IsValid()) {
    const uint32_t bitrate = mParser.CurrentFrame().Header().Bitrate();
    return NothingIfNegative(
        media::TimeUnit::FromSeconds(static_cast<double>(size) * 8 / bitrate));
  }

  if (AverageFrameLength() > 0) {
    numFrames = std::lround(AssertedCast<double>(size) / AverageFrameLength());
  }

  return NothingIfNegative(Duration(numFrames) - (EncoderDelay() + Padding()));
}

TimeUnit MP3TrackDemuxer::Duration(int64_t aNumFrames) const {
  if (!mSamplesPerSecond) {
    return TimeUnit::Invalid();
  }

  const int64_t frameCount = aNumFrames * mSamplesPerFrame;
  return TimeUnit(frameCount, mSamplesPerSecond);
}

MediaByteRange MP3TrackDemuxer::FindFirstFrame() {
  // We attempt to find multiple successive frames to avoid locking onto a false
  // positive if we're fed a stream that has been cut mid-frame.
  // For compatibility reasons we have to use the same frame count as Chrome,
  // since some web sites actually use a file that short to test our playback
  // capabilities.
  static const int MIN_SUCCESSIVE_FRAMES = 3;
  mFrameLock = false;

  MediaByteRange candidateFrame = FindNextFrame();
  int numSuccFrames = candidateFrame.Length() > 0;
  MediaByteRange currentFrame = candidateFrame;
  MP3LOGV("FindFirst() first candidate frame: mOffset=%" PRIu64
          " Length()=%" PRIu64,
          candidateFrame.mStart, candidateFrame.Length());

  while (candidateFrame.Length()) {
    mParser.EndFrameSession();
    mOffset = currentFrame.mEnd;
    const MediaByteRange prevFrame = currentFrame;

    // FindNextFrame() here will only return frames consistent with our
    // candidate frame.
    currentFrame = FindNextFrame();
    numSuccFrames += currentFrame.Length() > 0;
    // Multiple successive false positives, which wouldn't be caught by the
    // consistency checks alone, can be detected by wrong alignment (non-zero
    // gap between frames).
    const int64_t frameSeparation = currentFrame.mStart - prevFrame.mEnd;

    if (!currentFrame.Length() || frameSeparation != 0) {
      MP3LOGV(
          "FindFirst() not enough successive frames detected, "
          "rejecting candidate frame: successiveFrames=%d, last "
          "Length()=%" PRIu64 ", last frameSeparation=%" PRId64,
          numSuccFrames, currentFrame.Length(), frameSeparation);

      mParser.ResetFrameData();
      mOffset = candidateFrame.mStart + 1;
      candidateFrame = FindNextFrame();
      numSuccFrames = candidateFrame.Length() > 0;
      currentFrame = candidateFrame;
      MP3LOGV("FindFirst() new candidate frame: mOffset=%" PRIu64
              " Length()=%" PRIu64,
              candidateFrame.mStart, candidateFrame.Length());
    } else if (numSuccFrames >= MIN_SUCCESSIVE_FRAMES) {
      MP3LOG(
          "FindFirst() accepting candidate frame: "
          "successiveFrames=%d",
          numSuccFrames);
      mFrameLock = true;
      return candidateFrame;
    } else if (prevFrame.mStart == mParser.TotalID3HeaderSize() &&
               currentFrame.mEnd == StreamLength()) {
      // We accept streams with only two frames if both frames are valid. This
      // is to handle very short files and provide parity with Chrome. See
      // bug 1432195 for more information. This will not handle short files
      // with a trailing tag, but as of writing we lack infrastructure to
      // handle such tags.
      MP3LOG(
          "FindFirst() accepting candidate frame for short stream: "
          "successiveFrames=%d",
          numSuccFrames);
      mFrameLock = true;
      return candidateFrame;
    }
  }

  MP3LOG("FindFirst() no suitable first frame found");
  return candidateFrame;
}

static bool VerifyFrameConsistency(const FrameParser::Frame& aFrame1,
                                   const FrameParser::Frame& aFrame2) {
  const auto& h1 = aFrame1.Header();
  const auto& h2 = aFrame2.Header();

  return h1.IsValid() && h2.IsValid() && h1.Layer() == h2.Layer() &&
         h1.SlotSize() == h2.SlotSize() &&
         h1.SamplesPerFrame() == h2.SamplesPerFrame() &&
         h1.Channels() == h2.Channels() && h1.SampleRate() == h2.SampleRate() &&
         h1.RawVersion() == h2.RawVersion() &&
         h1.RawProtection() == h2.RawProtection();
}

MediaByteRange MP3TrackDemuxer::FindNextFrame() {
  static const int BUFFER_SIZE = 64;
  static const uint32_t MAX_SKIPPABLE_BYTES = 1024 * BUFFER_SIZE;

  MP3LOGV("FindNext() Begin mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d",
          mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels);

  uint8_t buffer[BUFFER_SIZE];
  uint32_t read = 0;

  bool foundFrame = false;
  int64_t frameHeaderOffset = 0;
  int64_t startOffset = mOffset;
  const bool searchingForID3 = !mParser.ID3Header().HasSizeBeenSet();

  // Check whether we've found a valid MPEG frame.
  while (!foundFrame) {
    // How many bytes we can go without finding a valid MPEG frame
    // (effectively rounded up to the next full buffer size multiple, as we
    // only check this before reading the next set of data into the buffer).

    // This default value of 0 will be used during testing whether we're being
    // fed a valid stream, which shouldn't have any gaps between frames.
    uint32_t maxSkippableBytes = 0;

    if (!mParser.FirstFrame().Length()) {
      // We're looking for the first valid frame. A well-formed file should
      // have its first frame header right at the start (skipping an ID3 tag
      // if necessary), but in order to support files that might have been
      // improperly cut, we search the first few kB for a frame header.
      maxSkippableBytes = MAX_SKIPPABLE_BYTES;
      // Since we're counting the skipped bytes from the offset we started
      // this parsing session with, we need to discount the ID3 tag size only
      // if we were looking for one during the current frame parsing session.
      if (searchingForID3) {
        maxSkippableBytes += mParser.TotalID3HeaderSize();
      }
    } else if (mFrameLock) {
      // We've found a valid MPEG stream, so don't impose any limits
      // to allow skipping corrupted data until we hit EOS.
      maxSkippableBytes = std::numeric_limits<uint32_t>::max();
    }

    if ((mOffset - startOffset > maxSkippableBytes) ||
        (read = Read(buffer, mOffset, BUFFER_SIZE)) == 0) {
      MP3LOG(
          "FindNext() EOS or exceeded maxSkippeableBytes without a frame "
          "(read: %d)",
          read);
      // This is not a valid MPEG audio stream or we've reached EOS, give up.
      break;
    }

    BufferReader reader(buffer, read);
    uint32_t bytesToSkip = 0;
    auto res = mParser.Parse(&reader, &bytesToSkip);
    foundFrame = res.unwrapOr(false);
    int64_t readerOffset = static_cast<int64_t>(reader.Offset());
    frameHeaderOffset = mOffset + readerOffset - FrameParser::FrameHeader::SIZE;

    // If we've found neither an MPEG frame header nor an ID3v2 tag,
    // the reader shouldn't have any bytes remaining.
    MOZ_ASSERT(foundFrame || bytesToSkip || !reader.Remaining());

    if (foundFrame && mParser.FirstFrame().Length() &&
        !VerifyFrameConsistency(mParser.FirstFrame(), mParser.CurrentFrame())) {
      MP3LOG("Skipping frame");
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
      mOffset += static_cast<int64_t>(read + bytesToSkip);
    }
  }

  if (StreamLength() != -1) {
    mEOS = frameHeaderOffset + mParser.CurrentFrame().Length() + BUFFER_SIZE >
           StreamLength();
  }

  if (!foundFrame || !mParser.CurrentFrame().Length()) {
    MP3LOG("FindNext() Exit foundFrame=%d mParser.CurrentFrame().Length()=%d ",
           foundFrame, mParser.CurrentFrame().Length());
    return {0, 0};
  }

  MP3LOGV("FindNext() End mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " frameHeaderOffset=%" PRId64
          " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d"
          " mChannels=%d, mEOS=%s",
          mOffset, mNumParsedFrames, mFrameIndex, frameHeaderOffset,
          mTotalFrameLen, mSamplesPerFrame, mSamplesPerSecond, mChannels,
          mEOS ? "true" : "false");

  return {frameHeaderOffset,
          frameHeaderOffset + mParser.CurrentFrame().Length()};
}

bool MP3TrackDemuxer::SkipNextFrame(const MediaByteRange& aRange) {
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

media::TimeUnit MP3TrackDemuxer::EncoderDelay() const {
  return media::TimeUnit(mEncoderDelay, mSamplesPerSecond);
}

uint32_t MP3TrackDemuxer::EncoderDelayFrames() const { return mEncoderDelay; }

media::TimeUnit MP3TrackDemuxer::Padding() const {
  return media::TimeUnit(mEncoderPadding, mSamplesPerSecond);
}

uint32_t MP3TrackDemuxer::PaddingFrames() const { return mEncoderPadding; }

already_AddRefed<MediaRawData> MP3TrackDemuxer::GetNextFrame(
    const MediaByteRange& aRange) {
  MP3LOG("GetNext() Begin({mStart=%" PRId64 " Length()=%" PRId64 "})",
         aRange.mStart, aRange.Length());
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> frame = new MediaRawData();
  frame->mOffset = aRange.mStart;

  UniquePtr<MediaRawDataWriter> frameWriter(frame->CreateWriter());
  if (!frameWriter->SetSize(static_cast<size_t>(aRange.Length()))) {
    MP3LOG("GetNext() Exit failed to allocated media buffer");
    return nullptr;
  }

  const uint32_t read =
      Read(frameWriter->Data(), frame->mOffset, frame->Size());

  if (read != aRange.Length()) {
    MP3LOG("GetNext() Exit read=%u frame->Size()=%zu", read, frame->Size());
    return nullptr;
  }

  UpdateState(aRange);

  if (mNumParsedFrames == 1) {
    // First frame parsed, let's read VBR info if available.
    BufferReader reader(frame->Data(), frame->Size());
    mFirstFrameOffset = frame->mOffset;

    if (mParser.ParseVBRHeader(&reader)) {
      // Parsing was successful
      if (mParser.VBRInfo().Type() == FrameParser::VBRHeader::XING) {
        MP3LOG("XING header present, skipping encoder delay (%u frames)",
               mParser.VBRInfo().EncoderDelay());
        mEncoderDelay = mParser.VBRInfo().EncoderDelay();
        mEncoderPadding = mParser.VBRInfo().EncoderPadding();
        // Padding is encoded as a 12-bit unsigned number so this is fine.
        mRemainingEncoderPadding = AssertedCast<int32_t>(mEncoderPadding);
        if (mEncoderDelay == 0) {
          // Skip the VBR frame + the decoder delay, that is always 529 frames
          // in practice for the decoder we're using.
          mEncoderDelay = mSamplesPerFrame + 529;
          MP3LOG(
              "No explicit delay present in vbr header, delay is assumed to be "
              "%u frames\n",
              mEncoderDelay);
        }
      } else if (mParser.VBRInfo().Type() == FrameParser::VBRHeader::VBRI) {
        MP3LOG("VBRI header present, skipping encoder delay (%u frames)",
               mParser.VBRInfo().EncoderDelay());
        mEncoderDelay = mParser.VBRInfo().EncoderDelay();
      }
    }
  }

  TimeUnit rawPts = Duration(mFrameIndex - 1) - EncoderDelay();
  TimeUnit rawDuration = Duration(1);
  TimeUnit rawEnd = rawPts + rawDuration;

  frame->mTime = std::max(TimeUnit::Zero(mSamplesPerSecond), rawPts);

  frame->mDuration = Duration(1);
  frame->mTimecode = frame->mTime;
  frame->mKeyframe = true;
  frame->mEOS = mEOS;

  // Handle decoder delay. A packet must be trimmed if its pts, adjusted for
  // decoder delay, is negative. A packet can be trimmed entirely.
  if (rawPts.IsNegative()) {
    frame->mDuration =
        std::max(TimeUnit::Zero(mSamplesPerSecond), rawEnd - frame->mTime);
  }

  // It's possible to create an mp3 file that has a padding value that somehow
  // spans multiple packets. In that case the duration is probably known,
  // because it's probably a VBR file with a XING header (that has a duration
  // field). Use the duration to be able to set the correct duration on
  // packets that aren't the last one.
  // For most files, the padding is less than a packet, it's simply substracted.
  if (mParser.VBRInfo().Type() == FrameParser::VBRHeader::XING &&
      mRemainingEncoderPadding > 0 &&
      frame->GetEndTime() > Duration().valueOr(TimeUnit::FromInfinity())) {
    TimeUnit duration = Duration().value();
    TimeUnit inPaddingZone = frame->GetEndTime() - duration;
    TimeUnit originalEnd = frame->GetEndTime();
    TimeUnit originalPts = frame->mTime;
    frame->mDuration -= inPaddingZone;
    // Packet is entirely padding and will be completely discarded by the
    // decoder.
    if (frame->mDuration.IsNegative()) {
      frame->mDuration = TimeUnit::Zero(mSamplesPerSecond);
    }
    int32_t paddingFrames =
        AssertedCast<int32_t>(inPaddingZone.ToTicksAtRate(mSamplesPerSecond));
    if (mRemainingEncoderPadding >= paddingFrames) {
      mRemainingEncoderPadding -= paddingFrames;
    } else {
      mRemainingEncoderPadding = 0;
    }
    MP3LOG("Trimming [%s, %s] to [%s,%s] (padding) (stream duration: %s)",
           originalPts.ToString().get(), originalEnd.ToString().get(),
           frame->mTime.ToString().get(), frame->GetEndTime().ToString().get(),
           duration.ToString().get());
  } else if (frame->mEOS &&
             mRemainingEncoderPadding <=
                 frame->mDuration.ToTicksAtRate(mSamplesPerSecond)) {
    frame->mDuration -= TimeUnit(mRemainingEncoderPadding, mSamplesPerSecond);
    MOZ_ASSERT(frame->mDuration.IsPositiveOrZero());
    MP3LOG("Trimming last packet %s to [%s,%s]", Padding().ToString().get(),
           frame->mTime.ToString().get(), frame->GetEndTime().ToString().get());
  }

  MP3LOGV("GetNext() End mOffset=%" PRIu64 " mNumParsedFrames=%" PRIu64
          " mFrameIndex=%" PRId64 " mTotalFrameLen=%" PRIu64
          " mSamplesPerFrame=%d mSamplesPerSecond=%d mChannels=%d, mEOS=%s",
          mOffset, mNumParsedFrames, mFrameIndex, mTotalFrameLen,
          mSamplesPerFrame, mSamplesPerSecond, mChannels,
          mEOS ? "true" : "false");

  // It's possible for the duration of a frame to be zero if the frame is to be
  // trimmed entirely because it's fully comprised of decoder delay samples.
  // This is common at the beginning of an stream.
  MOZ_ASSERT(frame->mDuration.IsPositiveOrZero());

  MP3LOG("Packet demuxed: pts [%s, %s] (duration: %s)",
         frame->mTime.ToString().get(), frame->GetEndTime().ToString().get(),
         frame->mDuration.ToString().get());

  // Indicate original packet information to trim after decoding.
  if (frame->mDuration != rawDuration) {
    frame->mOriginalPresentationWindow = Some(TimeInterval{rawPts, rawEnd});
    MP3LOG("Total packet time excluding trimming: [%s, %s]",
           rawPts.ToString().get(), rawEnd.ToString().get());
  }

  return frame.forget();
}

int64_t MP3TrackDemuxer::OffsetFromFrameIndex(int64_t aFrameIndex) const {
  int64_t offset = 0;
  const auto& vbr = mParser.VBRInfo();

  if (vbr.IsComplete()) {
    offset = mFirstFrameOffset + aFrameIndex * vbr.NumBytes().value() /
                                     vbr.NumAudioFrames().value();
  } else if (AverageFrameLength() > 0) {
    offset = mFirstFrameOffset +
             AssertedCast<int64_t>(static_cast<float>(aFrameIndex) *
                                   AverageFrameLength());
  }

  MP3LOGV("OffsetFromFrameIndex(%" PRId64 ") -> %" PRId64, aFrameIndex, offset);
  return std::max<int64_t>(mFirstFrameOffset, offset);
}

int64_t MP3TrackDemuxer::FrameIndexFromOffset(int64_t aOffset) const {
  int64_t frameIndex = 0;
  const auto& vbr = mParser.VBRInfo();

  if (vbr.IsComplete()) {
    frameIndex =
        AssertedCast<int64_t>(static_cast<float>(aOffset - mFirstFrameOffset) /
                              static_cast<float>(vbr.NumBytes().value()) *
                              static_cast<float>(vbr.NumAudioFrames().value()));
    frameIndex = std::min<int64_t>(vbr.NumAudioFrames().value(), frameIndex);
  } else if (AverageFrameLength() > 0) {
    frameIndex = AssertedCast<int64_t>(
        static_cast<float>(aOffset - mFirstFrameOffset) / AverageFrameLength());
  }

  MP3LOGV("FrameIndexFromOffset(%" PRId64 ") -> %" PRId64, aOffset, frameIndex);
  return std::max<int64_t>(0, frameIndex);
}

int64_t MP3TrackDemuxer::FrameIndexFromTime(
    const media::TimeUnit& aTime) const {
  int64_t frameIndex = 0;
  if (mSamplesPerSecond > 0 && mSamplesPerFrame > 0) {
    frameIndex = AssertedCast<int64_t>(
        aTime.ToSeconds() * mSamplesPerSecond / mSamplesPerFrame - 1);
  }

  MP3LOGV("FrameIndexFromOffset(%fs) -> %" PRId64, aTime.ToSeconds(),
          frameIndex);
  return std::max<int64_t>(0, frameIndex);
}

void MP3TrackDemuxer::UpdateState(const MediaByteRange& aRange) {
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

uint32_t MP3TrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset,
                               uint32_t aSize) {
  MP3LOGV("MP3TrackDemuxer::Read(%p %" PRId64 " %d)", aBuffer, aOffset, aSize);

  const int64_t streamLen = StreamLength();
  if (mInfo && streamLen > 0) {
    // Prevent blocking reads after successful initialization.
    int64_t max = streamLen > aOffset ? streamLen - aOffset : 0;
    aSize = std::min<int64_t>(aSize, max);
  }

  uint32_t read = 0;
  MP3LOGV("MP3TrackDemuxer::Read        -> ReadAt(%u)", aSize);
  const nsresult rv = mSource.ReadAt(aOffset, reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize), &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return read;
}

double MP3TrackDemuxer::AverageFrameLength() const {
  if (mNumParsedFrames) {
    return static_cast<double>(mTotalFrameLen) /
           static_cast<double>(mNumParsedFrames);
  }
  const auto& vbr = mParser.VBRInfo();
  if (vbr.IsComplete() && vbr.NumAudioFrames().value() + 1) {
    return static_cast<double>(vbr.NumBytes().value()) /
           (vbr.NumAudioFrames().value() + 1);
  }
  return 0.0;
}

Maybe<uint32_t> MP3TrackDemuxer::ValidNumAudioFrames() const {
  return mParser.VBRInfo().IsValid() &&
                 mParser.VBRInfo().NumAudioFrames().valueOr(0) + 1 > 1
             ? mParser.VBRInfo().NumAudioFrames()
             : Nothing();
}

}  // namespace mozilla

#undef MP3LOG
#undef MP3LOGV
