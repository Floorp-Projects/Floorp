/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerParser.h"

#include "WebMBufferedParser.h"
#include "mozilla/Endian.h"
#include "mp4_demuxer/BufferStream.h"
#include "mp4_demuxer/MoofParser.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_DEBUGV(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_DEBUGV(...)
#define MSE_API(...)
#endif

namespace mozilla {

bool
ContainerParser::IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength)
{
MSE_DEBUG("ContainerParser(%p)::IsInitSegmentPresent aLength=%u [%x%x%x%x]",
            this, aLength,
            aLength > 0 ? aData[0] : 0,
            aLength > 1 ? aData[1] : 0,
            aLength > 2 ? aData[2] : 0,
            aLength > 3 ? aData[3] : 0);
return false;
}

bool
ContainerParser::IsMediaSegmentPresent(const uint8_t* aData, uint32_t aLength)
{
  MSE_DEBUG("ContainerParser(%p)::IsMediaSegmentPresent aLength=%u [%x%x%x%x]",
            this, aLength,
            aLength > 0 ? aData[0] : 0,
            aLength > 1 ? aData[1] : 0,
            aLength > 2 ? aData[2] : 0,
            aLength > 3 ? aData[3] : 0);
  return false;
}

bool
ContainerParser::ParseStartAndEndTimestamps(const uint8_t* aData, uint32_t aLength,
                                            int64_t& aStart, int64_t& aEnd)
{
  return false;
}

bool
ContainerParser::TimestampsFuzzyEqual(int64_t aLhs, int64_t aRhs)
{
  return llabs(aLhs - aRhs) <= GetRoundingError();
}

int64_t
ContainerParser::GetRoundingError()
{
  NS_WARNING("Using default ContainerParser::GetRoundingError implementation");
  return 0;
}

const nsTArray<uint8_t>&
ContainerParser::InitData()
{
  MOZ_ASSERT(mHasInitData);
  return mInitData;
}

class WebMContainerParser : public ContainerParser {
public:
  WebMContainerParser()
    : mParser(0), mOffset(0)
  {}

  static const unsigned NS_PER_USEC = 1000;
  static const unsigned USEC_PER_SEC = 1000000;

  bool IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    ContainerParser::IsInitSegmentPresent(aData, aLength);
    // XXX: This is overly primitive, needs to collect data as it's appended
    // to the SB and handle, rather than assuming everything is present in a
    // single aData segment.
    // 0x1a45dfa3 // EBML
    // ...
    // DocType == "webm"
    // ...
    // 0x18538067 // Segment (must be "unknown" size)
    // 0x1549a966 // -> Segment Info
    // 0x1654ae6b // -> One or more Tracks
    if (aLength >= 4 &&
        aData[0] == 0x1a && aData[1] == 0x45 && aData[2] == 0xdf && aData[3] == 0xa3) {
      return true;
    }
    return false;
  }

  bool IsMediaSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    ContainerParser::IsMediaSegmentPresent(aData, aLength);
    // XXX: This is overly primitive, needs to collect data as it's appended
    // to the SB and handle, rather than assuming everything is present in a
    // single aData segment.
    // 0x1a45dfa3 // EBML
    // ...
    // DocType == "webm"
    // ...
    // 0x18538067 // Segment (must be "unknown" size)
    // 0x1549a966 // -> Segment Info
    // 0x1654ae6b // -> One or more Tracks
    if (aLength >= 4 &&
        aData[0] == 0x1f && aData[1] == 0x43 && aData[2] == 0xb6 && aData[3] == 0x75) {
      return true;
    }
    return false;
  }

  bool ParseStartAndEndTimestamps(const uint8_t* aData, uint32_t aLength,
                                  int64_t& aStart, int64_t& aEnd)
  {
    bool initSegment = IsInitSegmentPresent(aData, aLength);
    if (initSegment) {
      mOffset = 0;
      mParser = WebMBufferedParser(0);
      mOverlappedMapping.Clear();
    }

    // XXX if it only adds new mappings, overlapped but not available
    // (e.g. overlap < 0) frames are "lost" from the reported mappings here.
    nsTArray<WebMTimeDataOffset> mapping;
    mapping.AppendElements(mOverlappedMapping);
    mOverlappedMapping.Clear();
    ReentrantMonitor dummy("dummy");
    mParser.Append(aData, aLength, mapping, dummy);

    // XXX This is a bit of a hack.  Assume if there are no timecodes
    // present and it's an init segment that it's _just_ an init segment.
    // We should be more precise.
    if (initSegment) {
      uint32_t length = aLength;
      if (!mapping.IsEmpty()) {
        length = mapping[0].mSyncOffset;
        MOZ_ASSERT(length <= aLength);
      }
      MSE_DEBUG("WebMContainerParser(%p)::ParseStartAndEndTimestamps: Stashed init of %u bytes.",
                this, length);

      mInitData.ReplaceElementsAt(0, mInitData.Length(), aData, length);
      mHasInitData = true;
    }
    mOffset += aLength;

    if (mapping.IsEmpty()) {
      return false;
    }

    // Exclude frames that we don't enough data to cover the end of.
    uint32_t endIdx = mapping.Length() - 1;
    while (mOffset < mapping[endIdx].mEndOffset && endIdx > 0) {
      endIdx -= 1;
    }

    if (endIdx == 0) {
      return false;
    }

    uint64_t frameDuration = mapping[endIdx].mTimecode - mapping[endIdx - 1].mTimecode;
    aStart = mapping[0].mTimecode / NS_PER_USEC;
    aEnd = (mapping[endIdx].mTimecode + frameDuration) / NS_PER_USEC;

    MSE_DEBUG("WebMContainerParser(%p)::ParseStartAndEndTimestamps: [%lld, %lld] [fso=%lld, leo=%lld, l=%u endIdx=%u]",
              this, aStart, aEnd, mapping[0].mSyncOffset, mapping[endIdx].mEndOffset, mapping.Length(), endIdx);

    mapping.RemoveElementsAt(0, endIdx + 1);
    mOverlappedMapping.AppendElements(mapping);

    return true;
  }

  int64_t GetRoundingError()
  {
    int64_t error = mParser.GetTimecodeScale() / NS_PER_USEC;
    return error * 2;
  }

private:
  WebMBufferedParser mParser;
  nsTArray<WebMTimeDataOffset> mOverlappedMapping;
  int64_t mOffset;
};

class MP4ContainerParser : public ContainerParser {
public:
  MP4ContainerParser() :mMonitor("MP4ContainerParser Index Monitor") {}

  bool IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    ContainerParser::IsInitSegmentPresent(aData, aLength);
    // Each MP4 atom has a chunk size and chunk type. The root chunk in an MP4
    // file is the 'ftyp' atom followed by a file type. We just check for a
    // vaguely valid 'ftyp' atom.

    if (aLength < 8) {
      return false;
    }

    uint32_t chunk_size = BigEndian::readUint32(aData);
    if (chunk_size < 8) {
      return false;
    }

    return aData[4] == 'f' && aData[5] == 't' && aData[6] == 'y' &&
           aData[7] == 'p';
  }

  bool IsMediaSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    ContainerParser::IsMediaSegmentPresent(aData, aLength);
    if (aLength < 8) {
      return false;
    }

    uint32_t chunk_size = BigEndian::readUint32(aData);
    if (chunk_size < 8) {
      return false;
    }

    return (aData[4] == 'm' && aData[5] == 'o' && aData[6] == 'o' && aData[7] == 'f') ||
           (aData[4] == 's' && aData[5] == 't' && aData[6] == 'y' && aData[7] == 'p');
  }

  bool ParseStartAndEndTimestamps(const uint8_t* aData, uint32_t aLength,
                                  int64_t& aStart, int64_t& aEnd)
  {
    MonitorAutoLock mon(mMonitor); // We're not actually racing against anything,
                                   // but mParser requires us to hold a monitor.
    bool initSegment = IsInitSegmentPresent(aData, aLength);
    if (initSegment) {
      mStream = new mp4_demuxer::BufferStream();
      // We use a timestampOffset of 0 for ContainerParser, and require
      // consumers of ParseStartAndEndTimestamps to add their timestamp offset
      // manually. This allows the ContainerParser to be shared across different
      // timestampOffsets.
      mParser = new mp4_demuxer::MoofParser(mStream, 0, 0, &mMonitor);
    } else if (!mStream || !mParser) {
      return false;
    }

    mStream->AppendBytes(aData, aLength);
    nsTArray<MediaByteRange> byteRanges;
    byteRanges.AppendElement(mStream->GetByteRange());
    mParser->RebuildFragmentedIndex(byteRanges);

    if (initSegment) {
      const MediaByteRange& range = mParser->mInitRange;
      MSE_DEBUG("MP4ContainerParser(%p)::ParseStartAndEndTimestamps: Stashed init of %u bytes.",
                this, range.mEnd - range.mStart);

      mInitData.ReplaceElementsAt(0, mInitData.Length(),
                                  aData + range.mStart,
                                  range.mEnd - range.mStart);
      mHasInitData = true;
    }

    mp4_demuxer::Interval<mp4_demuxer::Microseconds> compositionRange =
      mParser->GetCompositionRange(byteRanges);

    mStream->DiscardBefore(mParser->mOffset);

    if (compositionRange.IsNull()) {
      return false;
    }
    aStart = compositionRange.start;
    aEnd = compositionRange.end;
    MSE_DEBUG("MP4ContainerParser(%p)::ParseStartAndEndTimestamps: [%lld, %lld]",
              this, aStart, aEnd);
    return true;
  }

  // Gaps of up to 20ms (marginally longer than a single frame at 60fps) are considered
  // to be sequential frames.
  int64_t GetRoundingError()
  {
    return 20000;
  }

private:
  nsRefPtr<mp4_demuxer::BufferStream> mStream;
  nsAutoPtr<mp4_demuxer::MoofParser> mParser;
  Monitor mMonitor;
};

/*static*/ ContainerParser*
ContainerParser::CreateForMIMEType(const nsACString& aType)
{
  if (aType.LowerCaseEqualsLiteral("video/webm") || aType.LowerCaseEqualsLiteral("audio/webm")) {
    return new WebMContainerParser();
  }

  if (aType.LowerCaseEqualsLiteral("video/mp4") || aType.LowerCaseEqualsLiteral("audio/mp4")) {
    return new MP4ContainerParser();
  }
  return new ContainerParser();
}

} // namespace mozilla
