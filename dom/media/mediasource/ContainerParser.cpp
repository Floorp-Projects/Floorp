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
#include "MediaData.h"
#ifdef MOZ_FMP4
#include "MP4Stream.h"
#include "mp4_demuxer/AtomType.h"
#include "mp4_demuxer/ByteReader.h"
#endif
#include "SourceBufferResource.h"

extern PRLogModuleInfo* GetMediaSourceSamplesLog();

/* Polyfill __func__ on MSVC to pass to the log. */
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

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
    if (aData->Length() >= 4 &&
        (*aData)[0] == 0x1f && (*aData)[1] == 0x43 && (*aData)[2] == 0xb6 &&
        (*aData)[3] == 0x75) {
      return true;
    }
    return false;
  }

  bool ParseStartAndEndTimestamps(MediaByteBuffer* aData,
                                  int64_t& aStart, int64_t& aEnd) override
  {
    bool initSegment = IsInitSegmentPresent(aData);
    if (initSegment) {
      mOffset = 0;
      mParser = WebMBufferedParser(0);
      mOverlappedMapping.Clear();
      mInitData = new MediaByteBuffer();
      mResource = new SourceBufferResource(NS_LITERAL_CSTRING("video/webm"));
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

    MSE_DEBUG(WebMContainerParser, "[%lld, %lld] [fso=%lld, leo=%lld, l=%u endIdx=%u]",
              aStart, aEnd, mapping[0].mSyncOffset, mapping[endIdx].mEndOffset, mapping.Length(), endIdx);

    mapping.RemoveElementsAt(0, endIdx + 1);
    mOverlappedMapping.AppendElements(mapping);

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
        MSE_DEBUGV(AtomParser ,"Checking atom:'%c%c%c%c'",
                   typec[0], typec[1], typec[2], typec[3]);
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
    nsTArray<MediaByteRange> byteRanges;
    MediaByteRange mbr =
      MediaByteRange(mParser->mOffset, mResource->GetLength());
    byteRanges.AppendElement(mbr);
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
  nsRefPtr<MP4Stream> mStream;
  nsAutoPtr<mp4_demuxer::MoofParser> mParser;
  Monitor mMonitor;
};
#endif

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
#endif
  return new ContainerParser(aType);
}

#undef MSE_DEBUG
#undef MSE_DEBUGV

} // namespace mozilla
