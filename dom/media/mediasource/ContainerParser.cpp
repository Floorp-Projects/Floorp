/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerParser.h"

#include "WebMBufferedParser.h"
#include "mozilla/Endian.h"
#include "mozilla/ErrorResult.h"
#include "mp4_demuxer/MoofParser.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "MediaData.h"
#ifdef MOZ_FMP4
#include "MP4Stream.h"
#include "mp4_demuxer/AtomType.h"
#include "mp4_demuxer/ByteReader.h"
#endif
#include "SourceBufferResource.h"

extern mozilla::LogModule* GetMediaSourceSamplesLog();

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define MSE_DEBUG(name, arg, ...) MOZ_LOG(GetMediaSourceSamplesLog(), mozilla::LogLevel::Debug, (TOSTRING(name) "(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(name, arg, ...) MOZ_LOG(GetMediaSourceSamplesLog(), mozilla::LogLevel::Verbose, (TOSTRING(name) "(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

namespace mozilla {

ContainerParser::ContainerParser(const nsACString& aType)
  : mHasInitData(false)
  , mType(aType)
{
}

ContainerParser::~ContainerParser() = default;

bool
ContainerParser::IsInitSegmentPresent(MediaByteBuffer* aData)
{
  MSE_DEBUG(ContainerParser, "aLength=%u [%x%x%x%x]",
            aData->Length(),
            aData->Length() > 0 ? (*aData)[0] : 0,
            aData->Length() > 1 ? (*aData)[1] : 0,
            aData->Length() > 2 ? (*aData)[2] : 0,
            aData->Length() > 3 ? (*aData)[3] : 0);
  return false;
}

bool
ContainerParser::IsMediaSegmentPresent(MediaByteBuffer* aData)
{
  MSE_DEBUG(ContainerParser, "aLength=%u [%x%x%x%x]",
            aData->Length(),
            aData->Length() > 0 ? (*aData)[0] : 0,
            aData->Length() > 1 ? (*aData)[1] : 0,
            aData->Length() > 2 ? (*aData)[2] : 0,
            aData->Length() > 3 ? (*aData)[3] : 0);
  return false;
}

bool
ContainerParser::ParseStartAndEndTimestamps(MediaByteBuffer* aData,
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

bool
ContainerParser::HasCompleteInitData()
{
  return mHasInitData && !!mInitData->Length();
}

MediaByteBuffer*
ContainerParser::InitData()
{
  return mInitData;
}

MediaByteRange
ContainerParser::InitSegmentRange()
{
  return mCompleteInitSegmentRange;
}

MediaByteRange
ContainerParser::MediaHeaderRange()
{
  return mCompleteMediaHeaderRange;
}

MediaByteRange
ContainerParser::MediaSegmentRange()
{
  return mCompleteMediaSegmentRange;
}

class WebMContainerParser : public ContainerParser {
public:
  explicit WebMContainerParser(const nsACString& aType)
    : ContainerParser(aType)
    , mParser(0)
    , mOffset(0)
  {}

  static const unsigned NS_PER_USEC = 1000;
  static const unsigned USEC_PER_SEC = 1000000;

  bool IsInitSegmentPresent(MediaByteBuffer* aData) override
  {
    ContainerParser::IsInitSegmentPresent(aData);
    // XXX: This is overly primitive, needs to collect data as it's appended
    // to the SB and handle, rather than assuming everything is present in a
    // single aData segment.
    // 0x1a45dfa3 // EBML
    // ...
    // DocType == "webm"
    // ...
    // 0x18538067 // Segment (must be "unknown" size or contain a value large
                  // enough to include the Segment Information and Tracks
                  // elements that follow)
    // 0x1549a966 // -> Segment Info
    // 0x1654ae6b // -> One or more Tracks

    // 0x1a45dfa3 // EBML
    if (aData->Length() >= 4 &&
        (*aData)[0] == 0x1a && (*aData)[1] == 0x45 && (*aData)[2] == 0xdf &&
        (*aData)[3] == 0xa3) {
      return true;
    }
    return false;
  }

  bool IsMediaSegmentPresent(MediaByteBuffer* aData) override
  {
    ContainerParser::IsMediaSegmentPresent(aData);
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

    // 0x1f43b675 // Cluster
    if (aData->Length() >= 4 &&
        (*aData)[0] == 0x1f && (*aData)[1] == 0x43 && (*aData)[2] == 0xb6 &&
        (*aData)[3] == 0x75) {
      return true;
    }
    // 0x1c53bb6b // Cues
    if (aData->Length() >= 4 &&
        (*aData)[0] == 0x1c && (*aData)[1] == 0x53 && (*aData)[2] == 0xbb &&
        (*aData)[3] == 0x6b) {
      return true;
    }
    return false;
  }

  bool ParseStartAndEndTimestamps(MediaByteBuffer* aData,
                                  int64_t& aStart, int64_t& aEnd) override
  {
    bool initSegment = IsInitSegmentPresent(aData);

    if (mLastMapping && (initSegment || IsMediaSegmentPresent(aData))) {
      // The last data contained a complete cluster but we can only detect it
      // now that a new one is starting.
      // We use mOffset as end position to ensure that any blocks not reported
      // by WebMBufferParser are properly skipped.
      mCompleteMediaSegmentRange = MediaByteRange(mLastMapping.ref().mSyncOffset,
                                                  mOffset);
      mLastMapping.reset();
      MSE_DEBUG(WebMContainerParser, "New cluster found at start, ending previous one");
      return false;
    }

    if (initSegment) {
      mOffset = 0;
      mParser = WebMBufferedParser(0);
      mOverlappedMapping.Clear();
      mInitData = new MediaByteBuffer();
      mResource = new SourceBufferResource(NS_LITERAL_CSTRING("video/webm"));
      mCompleteMediaHeaderRange = MediaByteRange();
      mCompleteMediaSegmentRange = MediaByteRange();
    }

    // XXX if it only adds new mappings, overlapped but not available
    // (e.g. overlap < 0) frames are "lost" from the reported mappings here.
    nsTArray<WebMTimeDataOffset> mapping;
    mapping.AppendElements(mOverlappedMapping);
    mOverlappedMapping.Clear();
    ReentrantMonitor dummy("dummy");
    mParser.Append(aData->Elements(), aData->Length(), mapping, dummy);
    if (mResource) {
      mResource->AppendData(aData);
    }

    // XXX This is a bit of a hack.  Assume if there are no timecodes
    // present and it's an init segment that it's _just_ an init segment.
    // We should be more precise.
    if (initSegment || !HasCompleteInitData()) {
      if (mParser.mInitEndOffset > 0) {
        MOZ_ASSERT(mParser.mInitEndOffset <= mResource->GetLength());
        if (!mInitData->SetLength(mParser.mInitEndOffset, fallible)) {
          // Super unlikely OOM
          return false;
        }
        mCompleteInitSegmentRange = MediaByteRange(0, mParser.mInitEndOffset);
        char* buffer = reinterpret_cast<char*>(mInitData->Elements());
        mResource->ReadFromCache(buffer, 0, mParser.mInitEndOffset);
        MSE_DEBUG(WebMContainerParser, "Stashed init of %u bytes.",
                  mParser.mInitEndOffset);
        mResource = nullptr;
      } else {
        MSE_DEBUG(WebMContainerParser, "Incomplete init found.");
      }
      mHasInitData = true;
    }
    mOffset += aData->Length();

    if (mapping.IsEmpty()) {
      return false;
    }

    // Calculate media range for first media segment.

    // Check if we have a cluster finishing in the current data.
    uint32_t endIdx = mapping.Length() - 1;
    bool foundNewCluster = false;
    while (mapping[0].mSyncOffset != mapping[endIdx].mSyncOffset) {
      endIdx -= 1;
      foundNewCluster = true;
    }

    int32_t completeIdx = endIdx;
    while (completeIdx >= 0 && mOffset < mapping[completeIdx].mEndOffset) {
      MSE_DEBUG(WebMContainerParser, "block is incomplete, missing: %lld",
                mapping[completeIdx].mEndOffset - mOffset);
      completeIdx -= 1;
    }

    // Save parsed blocks for which we do not have all data yet.
    mOverlappedMapping.AppendElements(mapping.Elements() + completeIdx + 1,
                                      mapping.Length() - completeIdx - 1);

    if (completeIdx < 0) {
      mLastMapping.reset();
      return false;
    }

    if (mCompleteMediaHeaderRange.IsEmpty()) {
      mCompleteMediaHeaderRange = MediaByteRange(mapping[0].mSyncOffset,
                                                 mapping[0].mEndOffset);
    }

    if (foundNewCluster && mOffset >= mapping[endIdx].mEndOffset) {
      // We now have all information required to delimit a complete cluster.
      int64_t endOffset = mapping[endIdx+1].mSyncOffset;
      if (mapping[endIdx+1].mInitOffset > mapping[endIdx].mInitOffset) {
        // We have a new init segment before this cluster.
        endOffset = mapping[endIdx+1].mInitOffset;
      }
      mCompleteMediaSegmentRange = MediaByteRange(mapping[endIdx].mSyncOffset,
                                                  endOffset);
    } else if (mapping[endIdx].mClusterEndOffset >= 0 &&
               mOffset >= mapping[endIdx].mClusterEndOffset) {
      mCompleteMediaSegmentRange = MediaByteRange(mapping[endIdx].mSyncOffset,
                                                  mParser.EndSegmentOffset(mapping[endIdx].mClusterEndOffset));
    }

    Maybe<WebMTimeDataOffset> previousMapping;
    if (completeIdx) {
      previousMapping = Some(mapping[completeIdx - 1]);
    } else {
      previousMapping = mLastMapping;
    }

    mLastMapping = Some(mapping[completeIdx]);

    if (!previousMapping && completeIdx + 1u >= mapping.Length()) {
      // We have no previous nor next block available,
      // so we can't estimate this block's duration.
      return false;
    }

    uint64_t frameDuration = (completeIdx + 1u < mapping.Length())
      ? mapping[completeIdx + 1].mTimecode - mapping[completeIdx].mTimecode
      : mapping[completeIdx].mTimecode - previousMapping.ref().mTimecode;
    aStart = mapping[0].mTimecode / NS_PER_USEC;
    aEnd = (mapping[completeIdx].mTimecode + frameDuration) / NS_PER_USEC;

    MSE_DEBUG(WebMContainerParser, "[%lld, %lld] [fso=%lld, leo=%lld, l=%u processedIdx=%u fs=%lld]",
              aStart, aEnd, mapping[0].mSyncOffset,
              mapping[completeIdx].mEndOffset, mapping.Length(), completeIdx,
              mCompleteMediaSegmentRange.mEnd);

    return true;
  }

  int64_t GetRoundingError() override
  {
    int64_t error = mParser.GetTimecodeScale() / NS_PER_USEC;
    return error * 2;
  }

private:
  WebMBufferedParser mParser;
  nsTArray<WebMTimeDataOffset> mOverlappedMapping;
  int64_t mOffset;
  Maybe<WebMTimeDataOffset> mLastMapping;
};

#ifdef MOZ_FMP4
class MP4ContainerParser : public ContainerParser {
public:
  explicit MP4ContainerParser(const nsACString& aType)
    : ContainerParser(aType)
    , mMonitor("MP4ContainerParser Index Monitor")
  {}

  bool IsInitSegmentPresent(MediaByteBuffer* aData) override
  {
    ContainerParser::IsInitSegmentPresent(aData);
    // Each MP4 atom has a chunk size and chunk type. The root chunk in an MP4
    // file is the 'ftyp' atom followed by a file type. We just check for a
    // vaguely valid 'ftyp' atom.
    AtomParser parser(mType, aData);
    return parser.StartWithInitSegment();
  }

  bool IsMediaSegmentPresent(MediaByteBuffer* aData) override
  {
    AtomParser parser(mType, aData);
    return parser.StartWithMediaSegment();
  }

private:
  class AtomParser {
  public:
    AtomParser(const nsACString& aType, const MediaByteBuffer* aData)
    {
      const nsCString mType(aType); // for logging macro.
      mp4_demuxer::ByteReader reader(aData);
      mp4_demuxer::AtomType initAtom("ftyp");
      mp4_demuxer::AtomType mediaAtom("moof");

      while (reader.Remaining() >= 8) {
        uint64_t size = reader.ReadU32();
        const uint8_t* typec = reader.Peek(4);
        uint32_t type = reader.ReadU32();
        MSE_DEBUGV(AtomParser ,"Checking atom:'%c%c%c%c' @ %u",
                   typec[0], typec[1], typec[2], typec[3],
                   (uint32_t)reader.Offset() - 8);
        if (mInitOffset.isNothing() &&
            mp4_demuxer::AtomType(type) == initAtom) {
          mInitOffset = Some(reader.Offset());
        }
        if (mMediaOffset.isNothing() &&
            mp4_demuxer::AtomType(type) == mediaAtom) {
          mMediaOffset = Some(reader.Offset());
        }
        if (mInitOffset.isSome() && mMediaOffset.isSome()) {
          // We have everything we need.
          break;
        }
        if (size == 1) {
          // 64 bits size.
          if (!reader.CanReadType<uint64_t>()) {
            break;
          }
          size = reader.ReadU64();
        } else if (size == 0) {
          // Atom extends to the end of the buffer, it can't have what we're
          // looking for.
          break;
        }
        if (reader.Remaining() < size - 8) {
          // Incomplete atom.
          break;
        }
        reader.Read(size - 8);
      }
      reader.DiscardRemaining();
    }

    bool StartWithInitSegment()
    {
      return mInitOffset.isSome() &&
        (mMediaOffset.isNothing() || mInitOffset.ref() < mMediaOffset.ref());
    }
    bool StartWithMediaSegment()
    {
      return mMediaOffset.isSome() &&
        (mInitOffset.isNothing() || mMediaOffset.ref() < mInitOffset.ref());
    }
  private:
    Maybe<size_t> mInitOffset;
    Maybe<size_t> mMediaOffset;
  };

public:
  bool ParseStartAndEndTimestamps(MediaByteBuffer* aData,
                                  int64_t& aStart, int64_t& aEnd) override
  {
    MonitorAutoLock mon(mMonitor); // We're not actually racing against anything,
                                   // but mParser requires us to hold a monitor.
    bool initSegment = IsInitSegmentPresent(aData);
    if (initSegment) {
      mResource = new SourceBufferResource(NS_LITERAL_CSTRING("video/mp4"));
      mStream = new MP4Stream(mResource);
      // We use a timestampOffset of 0 for ContainerParser, and require
      // consumers of ParseStartAndEndTimestamps to add their timestamp offset
      // manually. This allows the ContainerParser to be shared across different
      // timestampOffsets.
      mParser = new mp4_demuxer::MoofParser(mStream, 0, /* aIsAudio = */ false, &mMonitor);
      mInitData = new MediaByteBuffer();
    } else if (!mStream || !mParser) {
      return false;
    }

    mResource->AppendData(aData);
    MediaByteRangeSet byteRanges;
    byteRanges +=
      MediaByteRange(mParser->mOffset, mResource->GetLength());
    mParser->RebuildFragmentedIndex(byteRanges);

    if (initSegment || !HasCompleteInitData()) {
      MediaByteRange& range = mParser->mInitRange;
      if (range.Length()) {
        mCompleteInitSegmentRange = range;
        if (!mInitData->SetLength(range.Length(), fallible)) {
          // Super unlikely OOM
          return false;
        }
        char* buffer = reinterpret_cast<char*>(mInitData->Elements());
        mResource->ReadFromCache(buffer, range.mStart, range.Length());
        MSE_DEBUG(MP4ContainerParser ,"Stashed init of %u bytes.",
                  range.Length());
      } else {
        MSE_DEBUG(MP4ContainerParser, "Incomplete init found.");
      }
      mHasInitData = true;
    }

    mp4_demuxer::Interval<mp4_demuxer::Microseconds> compositionRange =
      mParser->GetCompositionRange(byteRanges);

    mCompleteMediaHeaderRange = mParser->FirstCompleteMediaHeader();
    mCompleteMediaSegmentRange = mParser->FirstCompleteMediaSegment();
    ErrorResult rv;
    if (HasCompleteInitData()) {
      mResource->EvictData(mParser->mOffset, mParser->mOffset, rv);
    }
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return false;
    }

    if (compositionRange.IsNull()) {
      return false;
    }
    aStart = compositionRange.start;
    aEnd = compositionRange.end;
    MSE_DEBUG(MP4ContainerParser, "[%lld, %lld]",
              aStart, aEnd);
    return true;
  }

  // Gaps of up to 35ms (marginally longer than a single frame at 30fps) are considered
  // to be sequential frames.
  int64_t GetRoundingError() override
  {
    return 35000;
  }

private:
  RefPtr<MP4Stream> mStream;
  nsAutoPtr<mp4_demuxer::MoofParser> mParser;
  Monitor mMonitor;
};
#endif // MOZ_FMP4

#ifdef MOZ_FMP4
class ADTSContainerParser : public ContainerParser {
public:
  explicit ADTSContainerParser(const nsACString& aType)
    : ContainerParser(aType)
  {}

  typedef struct {
    size_t header_length; // Length of just the initialization data.
    size_t frame_length;  // Includes header_length;
    uint8_t aac_frames;   // Number of AAC frames in the ADTS frame.
    bool have_crc;
  } Header;

  /// Helper to parse the ADTS header, returning data we care about.
  /// Returns true if the header is parsed successfully.
  /// Returns false if the header is invalid or incomplete,
  /// without modifying the passed-in Header object.
  bool Parse(MediaByteBuffer* aData, Header& header)
  {
    MOZ_ASSERT(aData);

    // ADTS initialization segments are just the packet header.
    if (aData->Length() < 7) {
      MSE_DEBUG(ADTSContainerParser, "buffer too short for header.");
      return false;
    }
    // Check 0xfffx sync word plus layer 0.
    if (((*aData)[0] != 0xff) || (((*aData)[1] & 0xf6) != 0xf0)) {
      MSE_DEBUG(ADTSContainerParser, "no syncword.");
      return false;
    }
    bool have_crc = !((*aData)[1] & 0x01);
    if (have_crc && aData->Length() < 9) {
      MSE_DEBUG(ADTSContainerParser, "buffer too short for header with crc.");
      return false;
    }
    uint8_t frequency_index = ((*aData)[2] & 0x3c) >> 2;
    MOZ_ASSERT(frequency_index < 16);
    if (frequency_index == 15) {
      MSE_DEBUG(ADTSContainerParser, "explicit frequency disallowed.");
      return false;
    }
    size_t header_length = have_crc ? 9 : 7;
    size_t data_length = (((*aData)[3] & 0x03) << 11) ||
                         (((*aData)[4] & 0xff) << 3) ||
                         (((*aData)[5] & 0xe0) >> 5);
    uint8_t frames = ((*aData)[6] & 0x03) + 1;
    MOZ_ASSERT(frames > 0);
    MOZ_ASSERT(frames < 4);

    // Return successfully parsed data.
    header.header_length = header_length;
    header.frame_length = header_length + data_length;
    header.aac_frames = frames;
    header.have_crc = have_crc;
    return true;
  }

  bool IsInitSegmentPresent(MediaByteBuffer* aData) override
  {
    // Call superclass for logging.
    ContainerParser::IsInitSegmentPresent(aData);

    Header header;
    if (!Parse(aData, header)) {
      return false;
    }

    MSE_DEBUGV(ADTSContainerParser, "%llu byte frame %d aac frames%s",
        (unsigned long long)header.frame_length, (int)header.aac_frames,
        header.have_crc ? " crc" : "");

    return true;
  }

  bool IsMediaSegmentPresent(MediaByteBuffer* aData) override
  {
    // Call superclass for logging.
    ContainerParser::IsMediaSegmentPresent(aData);

    // Make sure we have a header so we know how long the frame is.
    // NB this assumes the media segment buffer starts with an
    // initialization segment. Since every frame has an ADTS header
    // this is a normal place to divide packets, but we can re-parse
    // mInitData if we need to handle separate media segments.
    Header header;
    if (!Parse(aData, header)) {
      return false;
    }
    // We're supposed to return true as long as aData contains the
    // start of a media segment, whether or not it's complete. So
    // return true if we have any data beyond the header.
    if (aData->Length() <= header.header_length) {
      return false;
    }

    // We should have at least a partial frame.
    return true;
  }

  bool ParseStartAndEndTimestamps(MediaByteBuffer* aData,
                                  int64_t& aStart, int64_t& aEnd) override
  {
    // ADTS header.
    Header header;
    if (!Parse(aData, header)) {
      return false;
    }
    mHasInitData = true;
    mCompleteInitSegmentRange = MediaByteRange(0, header.header_length);

    // Cache raw header in case the caller wants a copy.
    mInitData = new MediaByteBuffer(header.header_length);
    mInitData->AppendElements(aData->Elements(), header.header_length);

    // Check that we have enough data for the frame body.
    if (aData->Length() < header.frame_length) {
      MSE_DEBUGV(ADTSContainerParser, "Not enough data for %llu byte frame"
          " in %llu byte buffer.",
          (unsigned long long)header.frame_length,
          (unsigned long long)(aData->Length()));
      return false;
    }
    mCompleteMediaSegmentRange = MediaByteRange(header.header_length,
                                                header.frame_length);
    // The ADTS MediaSource Byte Stream Format document doesn't
    // define media header. Just treat it the same as the whole
    // media segment.
    mCompleteMediaHeaderRange = mCompleteMediaSegmentRange;

    MSE_DEBUG(ADTSContainerParser, "[%lld, %lld]",
              aStart, aEnd);
    // We don't update timestamps, regardless.
    return false;
  }

  // Audio shouldn't have gaps.
  // Especially when we generate the timestamps ourselves.
  int64_t GetRoundingError() override
  {
    return 0;
  }
};
#endif // MOZ_FMP4

/*static*/ ContainerParser*
ContainerParser::CreateForMIMEType(const nsACString& aType)
{
  if (aType.LowerCaseEqualsLiteral("video/webm") || aType.LowerCaseEqualsLiteral("audio/webm")) {
    return new WebMContainerParser(aType);
  }

#ifdef MOZ_FMP4
  if (aType.LowerCaseEqualsLiteral("video/mp4") || aType.LowerCaseEqualsLiteral("audio/mp4")) {
    return new MP4ContainerParser(aType);
  }
  if (aType.LowerCaseEqualsLiteral("audio/aac")) {
    return new ADTSContainerParser(aType);
  }
#endif

  return new ContainerParser(aType);
}

#undef MSE_DEBUG
#undef MSE_DEBUGV

} // namespace mozilla
