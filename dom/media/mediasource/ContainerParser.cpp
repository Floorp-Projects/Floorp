/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerParser.h"

#include "WebMBufferedParser.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ErrorResult.h"
#include "MoofParser.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Try.h"
#include "MediaData.h"
#include "nsMimeTypes.h"
#include "AtomType.h"
#include "BufferReader.h"
#include "ByteStream.h"
#include "MP4Interval.h"
#include "SampleIterator.h"
#include "SourceBufferResource.h"
#include <algorithm>

extern mozilla::LogModule* GetMediaSourceSamplesLog();

#define MSE_DEBUG(arg, ...)                                            \
  DDMOZ_LOG(GetMediaSourceSamplesLog(), mozilla::LogLevel::Debug,      \
            "(%s)::%s: " arg, mType.OriginalString().Data(), __func__, \
            ##__VA_ARGS__)
#define MSE_DEBUGV(arg, ...)                                           \
  DDMOZ_LOG(GetMediaSourceSamplesLog(), mozilla::LogLevel::Verbose,    \
            "(%s)::%s: " arg, mType.OriginalString().Data(), __func__, \
            ##__VA_ARGS__)
#define MSE_DEBUGVEX(_this, arg, ...)                                        \
  DDMOZ_LOGEX(_this, GetMediaSourceSamplesLog(), mozilla::LogLevel::Verbose, \
              "(%s)::%s: " arg, mType.OriginalString().Data(), __func__,     \
              ##__VA_ARGS__)

namespace mozilla {

ContainerParser::ContainerParser(const MediaContainerType& aType)
    : mHasInitData(false), mTotalParsed(0), mGlobalOffset(0), mType(aType) {}

ContainerParser::~ContainerParser() = default;

MediaResult ContainerParser::IsInitSegmentPresent(const MediaSpan& aData) {
  MSE_DEBUG(
      "aLength=%zu [%x%x%x%x]", aData.Length(),
      aData.Length() > 0 ? aData[0] : 0, aData.Length() > 1 ? aData[1] : 0,
      aData.Length() > 2 ? aData[2] : 0, aData.Length() > 3 ? aData[3] : 0);
  return NS_ERROR_NOT_AVAILABLE;
}

MediaResult ContainerParser::IsMediaSegmentPresent(const MediaSpan& aData) {
  MSE_DEBUG(
      "aLength=%zu [%x%x%x%x]", aData.Length(),
      aData.Length() > 0 ? aData[0] : 0, aData.Length() > 1 ? aData[1] : 0,
      aData.Length() > 2 ? aData[2] : 0, aData.Length() > 3 ? aData[3] : 0);
  return NS_ERROR_NOT_AVAILABLE;
}

MediaResult ContainerParser::ParseStartAndEndTimestamps(const MediaSpan& aData,
                                                        media::TimeUnit& aStart,
                                                        media::TimeUnit& aEnd) {
  return NS_ERROR_NOT_AVAILABLE;
}

bool ContainerParser::TimestampsFuzzyEqual(int64_t aLhs, int64_t aRhs) {
  return llabs(aLhs - aRhs) <= GetRoundingError();
}

int64_t ContainerParser::GetRoundingError() {
  NS_WARNING("Using default ContainerParser::GetRoundingError implementation");
  return 0;
}

bool ContainerParser::HasCompleteInitData() {
  return mHasInitData && !!mInitData->Length();
}

MediaByteBuffer* ContainerParser::InitData() { return mInitData; }

MediaByteRange ContainerParser::InitSegmentRange() {
  return mCompleteInitSegmentRange;
}

MediaByteRange ContainerParser::MediaHeaderRange() {
  return mCompleteMediaHeaderRange;
}

MediaByteRange ContainerParser::MediaSegmentRange() {
  return mCompleteMediaSegmentRange;
}

DDLoggedTypeDeclNameAndBase(WebMContainerParser, ContainerParser);

class WebMContainerParser
    : public ContainerParser,
      public DecoderDoctorLifeLogger<WebMContainerParser> {
 public:
  explicit WebMContainerParser(const MediaContainerType& aType)
      : ContainerParser(aType), mParser(0), mOffset(0) {}

  static const unsigned NS_PER_USEC = 1000;

  MediaResult IsInitSegmentPresent(const MediaSpan& aData) override {
    ContainerParser::IsInitSegmentPresent(aData);
    if (aData.Length() < 4) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    WebMBufferedParser parser(0);
    nsTArray<WebMTimeDataOffset> mapping;
    if (auto result = parser.Append(aData.Elements(), aData.Length(), mapping);
        NS_FAILED(result)) {
      return result;
    }
    return parser.mInitEndOffset > 0 ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  MediaResult IsMediaSegmentPresent(const MediaSpan& aData) override {
    ContainerParser::IsMediaSegmentPresent(aData);
    if (aData.Length() < 4) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    WebMBufferedParser parser(0);
    nsTArray<WebMTimeDataOffset> mapping;
    parser.AppendMediaSegmentOnly();
    if (auto result = parser.Append(aData.Elements(), aData.Length(), mapping);
        NS_FAILED(result)) {
      return result;
    }
    return parser.GetClusterOffset() >= 0 ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  MediaResult ParseStartAndEndTimestamps(const MediaSpan& aData,
                                         media::TimeUnit& aStart,
                                         media::TimeUnit& aEnd) override {
    bool initSegment = NS_SUCCEEDED(IsInitSegmentPresent(aData));

    if (mLastMapping &&
        (initSegment || NS_SUCCEEDED(IsMediaSegmentPresent(aData)))) {
      // The last data contained a complete cluster but we can only detect it
      // now that a new one is starting.
      // We use mOffset as end position to ensure that any blocks not reported
      // by WebMBufferParser are properly skipped.
      mCompleteMediaSegmentRange =
          MediaByteRange(mLastMapping.ref().mSyncOffset, mOffset) +
          mGlobalOffset;
      mLastMapping.reset();
      MSE_DEBUG("New cluster found at start, ending previous one");
      return NS_ERROR_NOT_AVAILABLE;
    }

    if (initSegment) {
      mOffset = 0;
      mParser = WebMBufferedParser(0);
      mOverlappedMapping.Clear();
      mInitData = new MediaByteBuffer();
      mResource = new SourceBufferResource();
      DDLINKCHILD("resource", mResource.get());
      mCompleteInitSegmentRange = MediaByteRange();
      mCompleteMediaHeaderRange = MediaByteRange();
      mCompleteMediaSegmentRange = MediaByteRange();
      mGlobalOffset = mTotalParsed;
    }

    // XXX if it only adds new mappings, overlapped but not available
    // (e.g. overlap < 0) frames are "lost" from the reported mappings here.
    nsTArray<WebMTimeDataOffset> mapping;
    mapping.AppendElements(mOverlappedMapping);
    mOverlappedMapping.Clear();
    if (auto result = mParser.Append(aData.Elements(), aData.Length(), mapping);
        NS_FAILED(result)) {
      return result;
    }
    if (mResource) {
      mResource->AppendData(aData);
    }

    // XXX This is a bit of a hack.  Assume if there are no timecodes
    // present and it's an init segment that it's _just_ an init segment.
    // We should be more precise.
    if (initSegment || !HasCompleteInitData()) {
      if (mParser.mInitEndOffset > 0) {
        MOZ_DIAGNOSTIC_ASSERT(mInitData && mResource &&
                              mParser.mInitEndOffset <= mResource->GetLength());
        if (!mInitData->SetLength(mParser.mInitEndOffset, fallible)) {
          // Super unlikely OOM
          return NS_ERROR_OUT_OF_MEMORY;
        }
        mCompleteInitSegmentRange =
            MediaByteRange(0, mParser.mInitEndOffset) + mGlobalOffset;
        char* buffer = reinterpret_cast<char*>(mInitData->Elements());
        mResource->ReadFromCache(buffer, 0, mParser.mInitEndOffset);
        MSE_DEBUG("Stashed init of %" PRId64 " bytes.", mParser.mInitEndOffset);
        mResource = nullptr;
      } else {
        MSE_DEBUG("Incomplete init found.");
      }
      mHasInitData = true;
    }
    mOffset += aData.Length();
    mTotalParsed += aData.Length();

    if (mapping.IsEmpty()) {
      return NS_ERROR_NOT_AVAILABLE;
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
      MSE_DEBUG("block is incomplete, missing: %" PRId64,
                mapping[completeIdx].mEndOffset - mOffset);
      completeIdx -= 1;
    }

    // Save parsed blocks for which we do not have all data yet.
    mOverlappedMapping.AppendElements(mapping.Elements() + completeIdx + 1,
                                      mapping.Length() - completeIdx - 1);

    if (completeIdx < 0) {
      mLastMapping.reset();
      return NS_ERROR_NOT_AVAILABLE;
    }

    if (mCompleteMediaHeaderRange.IsEmpty()) {
      mCompleteMediaHeaderRange =
          MediaByteRange(mapping[0].mSyncOffset, mapping[0].mEndOffset) +
          mGlobalOffset;
    }

    if (foundNewCluster && mOffset >= mapping[endIdx].mEndOffset) {
      // We now have all information required to delimit a complete cluster.
      int64_t endOffset = mapping[endIdx + 1].mSyncOffset;
      if (mapping[endIdx + 1].mInitOffset > mapping[endIdx].mInitOffset) {
        // We have a new init segment before this cluster.
        endOffset = mapping[endIdx + 1].mInitOffset;
      }
      mCompleteMediaSegmentRange =
          MediaByteRange(mapping[endIdx].mSyncOffset, endOffset) +
          mGlobalOffset;
    } else if (mapping[endIdx].mClusterEndOffset >= 0 &&
               mOffset >= mapping[endIdx].mClusterEndOffset) {
      mCompleteMediaSegmentRange =
          MediaByteRange(
              mapping[endIdx].mSyncOffset,
              mParser.EndSegmentOffset(mapping[endIdx].mClusterEndOffset)) +
          mGlobalOffset;
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
      return NS_ERROR_NOT_AVAILABLE;
    }

    uint64_t frameDuration =
        (completeIdx + 1u < mapping.Length())
            ? mapping[completeIdx + 1].mTimecode -
                  mapping[completeIdx].mTimecode
            : mapping[completeIdx].mTimecode - previousMapping.ref().mTimecode;
    aStart = media::TimeUnit::FromNanoseconds(
        AssertedCast<int64_t>(mapping[0].mTimecode));
    aEnd = media::TimeUnit::FromNanoseconds(
        AssertedCast<int64_t>(mapping[completeIdx].mTimecode + frameDuration));

    MSE_DEBUG("[%" PRId64 ", %" PRId64 "] [fso=%" PRId64 ", leo=%" PRId64
              ", l=%zu processedIdx=%u fs=%" PRId64 "]",
              aStart.ToMicroseconds(), aEnd.ToMicroseconds(),
              mapping[0].mSyncOffset, mapping[completeIdx].mEndOffset,
              mapping.Length(), completeIdx, mCompleteMediaSegmentRange.mEnd);

    return NS_OK;
  }

  int64_t GetRoundingError() override {
    int64_t error = mParser.GetTimecodeScale() / NS_PER_USEC;
    return error * 2;
  }

 private:
  WebMBufferedParser mParser;
  nsTArray<WebMTimeDataOffset> mOverlappedMapping;
  int64_t mOffset;
  Maybe<WebMTimeDataOffset> mLastMapping;
};

DDLoggedTypeDeclNameAndBase(MP4Stream, ByteStream);

class MP4Stream : public ByteStream, public DecoderDoctorLifeLogger<MP4Stream> {
 public:
  explicit MP4Stream(SourceBufferResource* aResource);
  virtual ~MP4Stream();
  bool ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
              size_t* aBytesRead) override;
  bool CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                    size_t* aBytesRead) override;
  bool Length(int64_t* aSize) override;
  const uint8_t* GetContiguousAccess(int64_t aOffset, size_t aSize) override;

 private:
  RefPtr<SourceBufferResource> mResource;
};

MP4Stream::MP4Stream(SourceBufferResource* aResource) : mResource(aResource) {
  MOZ_COUNT_CTOR(MP4Stream);
  MOZ_ASSERT(aResource);
  DDLINKCHILD("resource", aResource);
}

MP4Stream::~MP4Stream() { MOZ_COUNT_DTOR(MP4Stream); }

bool MP4Stream::ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                       size_t* aBytesRead) {
  return CachedReadAt(aOffset, aBuffer, aCount, aBytesRead);
}

bool MP4Stream::CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                             size_t* aBytesRead) {
  nsresult rv = mResource->ReadFromCache(reinterpret_cast<char*>(aBuffer),
                                         aOffset, aCount);
  if (NS_FAILED(rv)) {
    *aBytesRead = 0;
    return false;
  }
  *aBytesRead = aCount;
  return true;
}

const uint8_t* MP4Stream::GetContiguousAccess(int64_t aOffset, size_t aSize) {
  return mResource->GetContiguousAccess(aOffset, aSize);
}

bool MP4Stream::Length(int64_t* aSize) {
  if (mResource->GetLength() < 0) return false;
  *aSize = mResource->GetLength();
  return true;
}

DDLoggedTypeDeclNameAndBase(MP4ContainerParser, ContainerParser);

class MP4ContainerParser : public ContainerParser,
                           public DecoderDoctorLifeLogger<MP4ContainerParser> {
 public:
  explicit MP4ContainerParser(const MediaContainerType& aType)
      : ContainerParser(aType) {}

  MediaResult IsInitSegmentPresent(const MediaSpan& aData) override {
    ContainerParser::IsInitSegmentPresent(aData);
    // Each MP4 atom has a chunk size and chunk type. The root chunk in an MP4
    // file is the 'ftyp' atom followed by a file type. We just check for a
    // vaguely valid 'ftyp' atom.
    if (aData.Length() < 8) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    AtomParser parser(*this, aData, AtomParser::StopAt::eInitSegment);
    if (!parser.IsValid()) {
      return MediaResult(
          NS_ERROR_FAILURE,
          RESULT_DETAIL("Invalid Top-Level Box:%s", parser.LastInvalidBox()));
    }
    return parser.StartWithInitSegment() ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  MediaResult IsMediaSegmentPresent(const MediaSpan& aData) override {
    if (aData.Length() < 8) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    AtomParser parser(*this, aData, AtomParser::StopAt::eMediaSegment);
    if (!parser.IsValid()) {
      return MediaResult(
          NS_ERROR_FAILURE,
          RESULT_DETAIL("Invalid Box:%s", parser.LastInvalidBox()));
    }
    return parser.StartWithMediaSegment() ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

 private:
  class AtomParser {
   public:
    enum class StopAt { eInitSegment, eMediaSegment, eEnd };

    AtomParser(const MP4ContainerParser& aParser, const MediaSpan& aData,
               StopAt aStop = StopAt::eEnd) {
      mValid = Init(aParser, aData, aStop).isOk();
    }

    Result<Ok, nsresult> Init(const MP4ContainerParser& aParser,
                              const MediaSpan& aData, StopAt aStop) {
      const MediaContainerType mType(
          aParser.ContainerType());  // for logging macro.
      BufferReader reader(aData);
      AtomType initAtom("moov");
      AtomType mediaAtom("moof");
      AtomType dataAtom("mdat");

      // Valid top-level boxes defined in ISO/IEC 14496-12 (Table 1)
      static const AtomType validBoxes[] = {
          "ftyp", "moov",          // init segment
          "pdin", "free", "sidx",  // optional prior moov box
          "styp", "moof", "mdat",  // media segment
          "mfra", "skip", "meta", "meco", "ssix", "prft",  // others.
          "pssh",         // optional with encrypted EME, though ignored.
          "emsg",         // ISO23009-1:2014 Section 5.10.3.3
          "bloc", "uuid"  // boxes accepted by chrome.
      };

      while (reader.Remaining() >= 8) {
        uint32_t tmp;
        MOZ_TRY_VAR(tmp, reader.ReadU32());
        uint64_t size = tmp;
        const uint8_t* typec = reader.Peek(4);
        MOZ_TRY_VAR(tmp, reader.ReadU32());
        AtomType type(tmp);
        MSE_DEBUGVEX(&aParser, "Checking atom:'%c%c%c%c' @ %u", typec[0],
                     typec[1], typec[2], typec[3],
                     (uint32_t)reader.Offset() - 8);
        if (std::find(std::begin(validBoxes), std::end(validBoxes), type) ==
            std::end(validBoxes)) {
          // No valid box found, no point continuing.
          mLastInvalidBox[0] = typec[0];
          mLastInvalidBox[1] = typec[1];
          mLastInvalidBox[2] = typec[2];
          mLastInvalidBox[3] = typec[3];
          mLastInvalidBox[4] = '\0';
          return Err(NS_ERROR_FAILURE);
        }
        if (mInitOffset.isNothing() && AtomType(type) == initAtom) {
          mInitOffset = Some(reader.Offset());
        }
        if (mMediaOffset.isNothing() && AtomType(type) == mediaAtom) {
          mMediaOffset = Some(reader.Offset());
        }
        if (mDataOffset.isNothing() && AtomType(type) == dataAtom) {
          mDataOffset = Some(reader.Offset());
        }
        if (size == 1) {
          // 64 bits size.
          MOZ_TRY_VAR(size, reader.ReadU64());
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

        if (aStop == StopAt::eInitSegment && (mInitOffset || mMediaOffset)) {
          // When we're looking for an init segment, if we encountered a media
          // segment, it we will need to be processed first. So we can stop
          // right away if we have found a media segment.
          break;
        }
        if (aStop == StopAt::eMediaSegment &&
            (mInitOffset || (mMediaOffset && mDataOffset))) {
          // When we're looking for a media segment, if we encountered an init
          // segment, it we will need to be processed first. So we can stop
          // right away if we have found an init segment.
          break;
        }
      }

      return Ok();
    }

    bool StartWithInitSegment() const {
      return mInitOffset.isSome() && (mMediaOffset.isNothing() ||
                                      mInitOffset.ref() < mMediaOffset.ref());
    }
    bool StartWithMediaSegment() const {
      return mMediaOffset.isSome() && (mInitOffset.isNothing() ||
                                       mMediaOffset.ref() < mInitOffset.ref());
    }
    bool IsValid() const { return mValid; }
    const char* LastInvalidBox() const { return mLastInvalidBox; }

   private:
    Maybe<size_t> mInitOffset;
    Maybe<size_t> mMediaOffset;
    Maybe<size_t> mDataOffset;
    bool mValid;
    char mLastInvalidBox[5];
  };

 public:
  MediaResult ParseStartAndEndTimestamps(const MediaSpan& aData,
                                         media::TimeUnit& aStart,
                                         media::TimeUnit& aEnd) override {
    bool initSegment = NS_SUCCEEDED(IsInitSegmentPresent(aData));
    if (initSegment) {
      mResource = new SourceBufferResource();
      DDLINKCHILD("resource", mResource.get());
      mStream = new MP4Stream(mResource);
      // We use a timestampOffset of 0 for ContainerParser, and require
      // consumers of ParseStartAndEndTimestamps to add their timestamp offset
      // manually. This allows the ContainerParser to be shared across different
      // timestampOffsets.
      mParser = MakeUnique<MoofParser>(mStream, AsVariant(ParseAllTracks{}),
                                       /* aIsAudio = */ false);
      DDLINKCHILD("parser", mParser.get());
      mInitData = new MediaByteBuffer();
      mCompleteInitSegmentRange = MediaByteRange();
      mCompleteMediaHeaderRange = MediaByteRange();
      mCompleteMediaSegmentRange = MediaByteRange();
      mGlobalOffset = mTotalParsed;
    } else if (!mStream || !mParser) {
      mTotalParsed += aData.Length();
      return NS_ERROR_NOT_AVAILABLE;
    }

    MOZ_DIAGNOSTIC_ASSERT(mResource && mParser && mInitData,
                          "Should have received an init segment first");

    mResource->AppendData(aData);
    MediaByteRangeSet byteRanges;
    byteRanges +=
        MediaByteRange(int64_t(mParser->mOffset), mResource->GetLength());
    mParser->RebuildFragmentedIndex(byteRanges);

    if (initSegment || !HasCompleteInitData()) {
      MediaByteRange& range = mParser->mInitRange;
      if (range.Length()) {
        mCompleteInitSegmentRange = range + mGlobalOffset;
        if (!mInitData->SetLength(range.Length(), fallible)) {
          // Super unlikely OOM
          return NS_ERROR_OUT_OF_MEMORY;
        }
        char* buffer = reinterpret_cast<char*>(mInitData->Elements());
        mResource->ReadFromCache(buffer, range.mStart, range.Length());
        MSE_DEBUG("Stashed init of %" PRIu64 " bytes.", range.Length());
      } else {
        MSE_DEBUG("Incomplete init found.");
      }
      mHasInitData = true;
    }
    mTotalParsed += aData.Length();

    MP4Interval<media::TimeUnit> compositionRange =
        mParser->GetCompositionRange(byteRanges);

    mCompleteMediaHeaderRange =
        mParser->FirstCompleteMediaHeader() + mGlobalOffset;
    mCompleteMediaSegmentRange =
        mParser->FirstCompleteMediaSegment() + mGlobalOffset;

    if (HasCompleteInitData()) {
      mResource->EvictData(mParser->mOffset, mParser->mOffset);
    }

    if (compositionRange.IsNull()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    aStart = compositionRange.start;
    aEnd = compositionRange.end;
    MSE_DEBUG("[%" PRId64 ", %" PRId64 "]", aStart.ToMicroseconds(),
              aEnd.ToMicroseconds());
    return NS_OK;
  }

  // Gaps of up to 35ms (marginally longer than a single frame at 30fps) are
  // considered to be sequential frames.
  int64_t GetRoundingError() override { return 35000; }

 private:
  RefPtr<MP4Stream> mStream;
  UniquePtr<MoofParser> mParser;
};
DDLoggedTypeDeclNameAndBase(ADTSContainerParser, ContainerParser);

class ADTSContainerParser
    : public ContainerParser,
      public DecoderDoctorLifeLogger<ADTSContainerParser> {
 public:
  explicit ADTSContainerParser(const MediaContainerType& aType)
      : ContainerParser(aType) {}

  typedef struct {
    size_t header_length;  // Length of just the initialization data.
    size_t frame_length;   // Includes header_length;
    uint8_t aac_frames;    // Number of AAC frames in the ADTS frame.
    bool have_crc;
  } Header;

  /// Helper to parse the ADTS header, returning data we care about.
  /// Returns true if the header is parsed successfully.
  /// Returns false if the header is invalid or incomplete,
  /// without modifying the passed-in Header object.
  bool Parse(const MediaSpan& aData, Header& header) {
    // ADTS initialization segments are just the packet header.
    if (aData.Length() < 7) {
      MSE_DEBUG("buffer too short for header.");
      return false;
    }
    // Check 0xfffx sync word plus layer 0.
    if ((aData[0] != 0xff) || ((aData[1] & 0xf6) != 0xf0)) {
      MSE_DEBUG("no syncword.");
      return false;
    }
    bool have_crc = !(aData[1] & 0x01);
    if (have_crc && aData.Length() < 9) {
      MSE_DEBUG("buffer too short for header with crc.");
      return false;
    }
    uint8_t frequency_index = (aData[2] & 0x3c) >> 2;
    MOZ_ASSERT(frequency_index < 16);
    if (frequency_index == 15) {
      MSE_DEBUG("explicit frequency disallowed.");
      return false;
    }
    size_t header_length = have_crc ? 9 : 7;
    size_t data_length = ((aData[3] & 0x03) << 11) | ((aData[4] & 0xff) << 3) |
                         ((aData[5] & 0xe0) >> 5);
    uint8_t frames = (aData[6] & 0x03) + 1;
    MOZ_ASSERT(frames > 0);
    MOZ_ASSERT(frames < 4);

    // Return successfully parsed data.
    header.header_length = header_length;
    header.frame_length = header_length + data_length;
    header.aac_frames = frames;
    header.have_crc = have_crc;
    return true;
  }

  MediaResult IsInitSegmentPresent(const MediaSpan& aData) override {
    // Call superclass for logging.
    ContainerParser::IsInitSegmentPresent(aData);

    Header header;
    if (!Parse(aData, header)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    MSE_DEBUGV("%llu byte frame %d aac frames%s",
               (unsigned long long)header.frame_length, (int)header.aac_frames,
               header.have_crc ? " crc" : "");

    return NS_OK;
  }

  MediaResult IsMediaSegmentPresent(const MediaSpan& aData) override {
    // Call superclass for logging.
    ContainerParser::IsMediaSegmentPresent(aData);

    // Make sure we have a header so we know how long the frame is.
    // NB this assumes the media segment buffer starts with an
    // initialization segment. Since every frame has an ADTS header
    // this is a normal place to divide packets, but we can re-parse
    // mInitData if we need to handle separate media segments.
    Header header;
    if (!Parse(aData, header)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    // We're supposed to return true as long as aData contains the
    // start of a media segment, whether or not it's complete. So
    // return true if we have any data beyond the header.
    if (aData.Length() <= header.header_length) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // We should have at least a partial frame.
    return NS_OK;
  }

  MediaResult ParseStartAndEndTimestamps(const MediaSpan& aData,
                                         media::TimeUnit& aStart,
                                         media::TimeUnit& aEnd) override {
    // ADTS header.
    Header header;
    if (!Parse(aData, header)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    mHasInitData = true;
    mCompleteInitSegmentRange =
        MediaByteRange(0, int64_t(header.header_length));

    // Cache raw header in case the caller wants a copy.
    mInitData = new MediaByteBuffer(header.header_length);
    mInitData->AppendElements(aData.Elements(), header.header_length);

    // Check that we have enough data for the frame body.
    if (aData.Length() < header.frame_length) {
      MSE_DEBUGV(
          "Not enough data for %llu byte frame"
          " in %llu byte buffer.",
          (unsigned long long)header.frame_length,
          (unsigned long long)(aData.Length()));
      return NS_ERROR_NOT_AVAILABLE;
    }
    mCompleteMediaSegmentRange =
        MediaByteRange(header.header_length, header.frame_length);
    // The ADTS MediaSource Byte Stream Format document doesn't
    // define media header. Just treat it the same as the whole
    // media segment.
    mCompleteMediaHeaderRange = mCompleteMediaSegmentRange;

    MSE_DEBUG("[%" PRId64 ", %" PRId64 "]", aStart.ToMicroseconds(),
              aEnd.ToMicroseconds());
    // We don't update timestamps, regardless.
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Audio shouldn't have gaps.
  // Especially when we generate the timestamps ourselves.
  int64_t GetRoundingError() override { return 0; }
};

/*static*/
UniquePtr<ContainerParser> ContainerParser::CreateForMIMEType(
    const MediaContainerType& aType) {
  if (aType.Type() == MEDIAMIMETYPE(VIDEO_WEBM) ||
      aType.Type() == MEDIAMIMETYPE(AUDIO_WEBM)) {
    return MakeUnique<WebMContainerParser>(aType);
  }

  if (aType.Type() == MEDIAMIMETYPE(VIDEO_MP4) ||
      aType.Type() == MEDIAMIMETYPE(AUDIO_MP4)) {
    return MakeUnique<MP4ContainerParser>(aType);
  }
  if (aType.Type() == MEDIAMIMETYPE("audio/aac")) {
    return MakeUnique<ADTSContainerParser>(aType);
  }

  return MakeUnique<ContainerParser>(aType);
}

#undef MSE_DEBUG
#undef MSE_DEBUGV
#undef MSE_DEBUGVEX

}  // namespace mozilla
