/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoofParser.h"
#include "Box.h"
#include "SinfParser.h"
#include <limits>
#include "MP4Interval.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Try.h"

extern mozilla::LogModule* GetDemuxerLog();

#define LOG_ERROR(name, arg, ...)                \
  MOZ_LOG(                                       \
      GetDemuxerLog(), mozilla::LogLevel::Error, \
      (MOZ_STRINGIFY(name) "(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOG_WARN(name, arg, ...)                   \
  MOZ_LOG(                                         \
      GetDemuxerLog(), mozilla::LogLevel::Warning, \
      (MOZ_STRINGIFY(name) "(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOG_DEBUG(name, arg, ...)                \
  MOZ_LOG(                                       \
      GetDemuxerLog(), mozilla::LogLevel::Debug, \
      (MOZ_STRINGIFY(name) "(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using TimeUnit = media::TimeUnit;

const uint32_t kKeyIdSize = 16;

bool MoofParser::RebuildFragmentedIndex(const MediaByteRangeSet& aByteRanges) {
  BoxContext context(mSource, aByteRanges);
  return RebuildFragmentedIndex(context);
}

bool MoofParser::RebuildFragmentedIndex(const MediaByteRangeSet& aByteRanges,
                                        bool* aCanEvict) {
  MOZ_ASSERT(aCanEvict);
  if (*aCanEvict && mMoofs.Length() > 1) {
    MOZ_ASSERT(mMoofs.Length() == mMediaRanges.Length());
    mMoofs.RemoveElementsAt(0, mMoofs.Length() - 1);
    mMediaRanges.RemoveElementsAt(0, mMediaRanges.Length() - 1);
    *aCanEvict = true;
  } else {
    *aCanEvict = false;
  }
  return RebuildFragmentedIndex(aByteRanges);
}

bool MoofParser::RebuildFragmentedIndex(BoxContext& aContext) {
  LOG_DEBUG(
      Moof,
      "Starting, mTrackParseMode=%s, track#=%" PRIu32
      " (ignore if multitrack).",
      mTrackParseMode.is<ParseAllTracks>() ? "multitrack" : "single track",
      mTrackParseMode.is<ParseAllTracks>() ? 0
                                           : mTrackParseMode.as<uint32_t>());
  bool foundValidMoof = false;

  for (Box box(&aContext, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moov") && mInitRange.IsEmpty()) {
      mInitRange = MediaByteRange(0, box.Range().mEnd);
      ParseMoov(box);
    } else if (box.IsType("moof")) {
      Moof moof(box, mTrackParseMode, mTrex, mMvhd, mMdhd, mEdts, mSinf,
                &mLastDecodeTime, mIsAudio, mTracksEndCts);

      if (!moof.IsValid()) {
        if (!box.Next().IsAvailable()) {
          // Abort search for now, without advancing mOffset so that parsing
          // can be attempted again when more of the resource is available.
          LOG_WARN(Moof, "Invalid moof. moof may not be complete yet.");
          break;
        }
        // moof is complete but invalid.  Skip to next box.
        continue;
      }

      if (!mMoofs.IsEmpty()) {
        // Stitch time ranges together in the case of a (hopefully small) time
        // range gap between moofs.
        mMoofs.LastElement().FixRounding(moof);
      }

      mMediaRanges.AppendElement(moof.mRange);
      mMoofs.AppendElement(std::move(moof));
      foundValidMoof = true;
    } else if (box.IsType("mdat") && !Moofs().IsEmpty()) {
      // Check if we have all our data from last moof.
      Moof& moof = Moofs().LastElement();
      media::Interval<int64_t> datarange(moof.mMdatRange.mStart,
                                         moof.mMdatRange.mEnd, 0);
      media::Interval<int64_t> mdat(box.Range().mStart, box.Range().mEnd, 0);
      if (datarange.Intersects(mdat)) {
        mMediaRanges.LastElement() =
            mMediaRanges.LastElement().Span(box.Range());
      }
    }
    mOffset = box.NextOffset();
  }
  MOZ_ASSERT(mTrackParseMode.is<ParseAllTracks>() ||
                 mTrex.mTrackId == mTrackParseMode.as<uint32_t>(),
             "If not parsing all tracks, mTrex should have the same track id "
             "as the track being parsed.");
  LOG_DEBUG(Moof, "Done, foundValidMoof=%s.",
            foundValidMoof ? "true" : "false");
  return foundValidMoof;
}

MediaByteRange MoofParser::FirstCompleteMediaHeader() {
  if (Moofs().IsEmpty()) {
    return MediaByteRange();
  }
  return Moofs()[0].mRange;
}

MediaByteRange MoofParser::FirstCompleteMediaSegment() {
  for (uint32_t i = 0; i < mMediaRanges.Length(); i++) {
    if (mMediaRanges[i].Contains(Moofs()[i].mMdatRange)) {
      return mMediaRanges[i];
    }
  }
  return MediaByteRange();
}

DDLoggedTypeDeclNameAndBase(BlockingStream, ByteStream);

class BlockingStream : public ByteStream,
                       public DecoderDoctorLifeLogger<BlockingStream> {
 public:
  explicit BlockingStream(ByteStream* aStream) : mStream(aStream) {
    DDLINKCHILD("stream", aStream);
  }

  bool ReadAt(int64_t offset, void* data, size_t size,
              size_t* bytes_read) override {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  bool CachedReadAt(int64_t offset, void* data, size_t size,
                    size_t* bytes_read) override {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  virtual bool Length(int64_t* size) override { return mStream->Length(size); }

 private:
  RefPtr<ByteStream> mStream;
};

bool MoofParser::BlockingReadNextMoof() {
  LOG_DEBUG(Moof, "Starting.");
  int64_t length = std::numeric_limits<int64_t>::max();
  mSource->Length(&length);
  RefPtr<BlockingStream> stream = new BlockingStream(mSource);
  MediaByteRangeSet byteRanges(MediaByteRange(0, length));

  BoxContext context(stream, byteRanges);
  for (Box box(&context, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moof")) {
      MediaByteRangeSet parseByteRanges(
          MediaByteRange(mOffset, box.Range().mEnd));
      BoxContext parseContext(stream, parseByteRanges);
      if (RebuildFragmentedIndex(parseContext)) {
        LOG_DEBUG(Moof, "Succeeded on RebuildFragmentedIndex, returning true.");
        return true;
      }
    }
  }
  LOG_DEBUG(Moof, "Couldn't read next moof, returning false.");
  return false;
}

void MoofParser::ScanForMetadata(mozilla::MediaByteRange& aMoov) {
  LOG_DEBUG(Moof, "Starting.");
  int64_t length = std::numeric_limits<int64_t>::max();
  mSource->Length(&length);
  MediaByteRangeSet byteRanges;
  byteRanges += MediaByteRange(0, length);
  RefPtr<BlockingStream> stream = new BlockingStream(mSource);

  BoxContext context(stream, byteRanges);
  for (Box box(&context, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moov")) {
      aMoov = box.Range();
      break;
    }
  }
  mInitRange = aMoov;
  LOG_DEBUG(Moof,
            "Done, mInitRange.mStart=%" PRIi64 ", mInitRange.mEnd=%" PRIi64,
            mInitRange.mStart, mInitRange.mEnd);
}

already_AddRefed<mozilla::MediaByteBuffer> MoofParser::Metadata() {
  LOG_DEBUG(Moof, "Starting.");
  MediaByteRange moov;
  ScanForMetadata(moov);
  CheckedInt<MediaByteBuffer::size_type> moovLength = moov.Length();
  if (!moovLength.isValid() || !moovLength.value()) {
    // No moov, or cannot be used as array size.
    LOG_WARN(Moof,
             "Did not get usable moov length while trying to parse Metadata.");
    return nullptr;
  }

  RefPtr<MediaByteBuffer> metadata = new MediaByteBuffer();
  if (!metadata->SetLength(moovLength.value(), fallible)) {
    LOG_ERROR(Moof, "OOM");
    return nullptr;
  }

  RefPtr<BlockingStream> stream = new BlockingStream(mSource);
  size_t read;
  bool rv = stream->ReadAt(moov.mStart, metadata->Elements(),
                           moovLength.value(), &read);
  if (!rv || read != moovLength.value()) {
    LOG_WARN(Moof, "Failed to read moov while trying to parse Metadata.");
    return nullptr;
  }
  LOG_DEBUG(Moof, "Done, found metadata.");
  return metadata.forget();
}

MP4Interval<TimeUnit> MoofParser::GetCompositionRange(
    const MediaByteRangeSet& aByteRanges) {
  LOG_DEBUG(Moof, "Starting.");
  MP4Interval<TimeUnit> compositionRange;
  BoxContext context(mSource, aByteRanges);
  for (size_t i = 0; i < mMoofs.Length(); i++) {
    Moof& moof = mMoofs[i];
    Box box(&context, moof.mRange.mStart);
    if (box.IsAvailable()) {
      compositionRange = compositionRange.Extents(moof.mTimeRange);
    }
  }
  LOG_DEBUG(Moof,
            "Done, compositionRange.start=%" PRIi64
            ", compositionRange.end=%" PRIi64 ".",
            compositionRange.start.ToMicroseconds(),
            compositionRange.end.ToMicroseconds());
  return compositionRange;
}

bool MoofParser::ReachedEnd() {
  int64_t length;
  return mSource->Length(&length) && mOffset == length;
}

void MoofParser::ParseMoov(Box& aBox) {
  LOG_DEBUG(Moof, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mvhd")) {
      mMvhd = Mvhd(box);
    } else if (box.IsType("trak")) {
      ParseTrak(box);
    } else if (box.IsType("mvex")) {
      ParseMvex(box);
    }
  }
  LOG_DEBUG(Moof, "Done.");
}

void MoofParser::ParseTrak(Box& aBox) {
  LOG_DEBUG(Trak, "Starting.");
  Tkhd tkhd;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tkhd")) {
      tkhd = Tkhd(box);
    } else if (box.IsType("mdia")) {
      if (mTrackParseMode.is<ParseAllTracks>() ||
          tkhd.mTrackId == mTrackParseMode.as<uint32_t>()) {
        ParseMdia(box);
      }
    } else if (box.IsType("edts") &&
               (mTrackParseMode.is<ParseAllTracks>() ||
                tkhd.mTrackId == mTrackParseMode.as<uint32_t>())) {
      mEdts = Edts(box);
    }
  }
  LOG_DEBUG(Trak, "Done.");
}

void MoofParser::ParseMdia(Box& aBox) {
  LOG_DEBUG(Mdia, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mdhd")) {
      mMdhd = Mdhd(box);
    } else if (box.IsType("minf")) {
      ParseMinf(box);
    }
  }
  LOG_DEBUG(Mdia, "Done.");
}

void MoofParser::ParseMvex(Box& aBox) {
  LOG_DEBUG(Mvex, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trex")) {
      Trex trex = Trex(box);
      if (mTrackParseMode.is<ParseAllTracks>() ||
          trex.mTrackId == mTrackParseMode.as<uint32_t>()) {
        mTrex = trex;
      }
    }
  }
  LOG_DEBUG(Mvex, "Done.");
}

void MoofParser::ParseMinf(Box& aBox) {
  LOG_DEBUG(Minf, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stbl")) {
      ParseStbl(box);
    }
  }
  LOG_DEBUG(Minf, "Done.");
}

void MoofParser::ParseStbl(Box& aBox) {
  LOG_DEBUG(Stbl, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stsd")) {
      ParseStsd(box);
    } else if (box.IsType("sgpd")) {
      Sgpd sgpd(box);
      if (sgpd.IsValid() && sgpd.mGroupingType == "seig") {
        mTrackSampleEncryptionInfoEntries.Clear();
        if (!mTrackSampleEncryptionInfoEntries.AppendElements(
                sgpd.mEntries, mozilla::fallible)) {
          LOG_ERROR(Stbl, "OOM");
          return;
        }
      }
    } else if (box.IsType("sbgp")) {
      Sbgp sbgp(box);
      if (sbgp.IsValid() && sbgp.mGroupingType == "seig") {
        mTrackSampleToGroupEntries.Clear();
        if (!mTrackSampleToGroupEntries.AppendElements(sbgp.mEntries,
                                                       mozilla::fallible)) {
          LOG_ERROR(Stbl, "OOM");
          return;
        }
      }
    }
  }
  LOG_DEBUG(Stbl, "Done.");
}

void MoofParser::ParseStsd(Box& aBox) {
  LOG_DEBUG(Stsd, "Starting.");
  if (mTrackParseMode.is<ParseAllTracks>()) {
    // It is not a sane operation to try and map sample description boxes from
    // multiple tracks onto the parser, which is modeled around storing metadata
    // for a single track.
    LOG_DEBUG(Stsd, "Early return due to multitrack parser.");
    return;
  }
  MOZ_ASSERT(
      mSampleDescriptions.IsEmpty(),
      "Shouldn't have any sample descriptions yet when starting to parse stsd");
  uint32_t numberEncryptedEntries = 0;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    SampleDescriptionEntry sampleDescriptionEntry{false};
    if (box.IsType("encv") || box.IsType("enca")) {
      ParseEncrypted(box);
      sampleDescriptionEntry.mIsEncryptedEntry = true;
      numberEncryptedEntries++;
    }
    if (!mSampleDescriptions.AppendElement(sampleDescriptionEntry,
                                           mozilla::fallible)) {
      LOG_ERROR(Stsd, "OOM");
      return;
    }
  }
  if (mSampleDescriptions.IsEmpty()) {
    LOG_WARN(Stsd,
             "No sample description entries found while parsing Stsd! This "
             "shouldn't happen, as the spec requires one for each track!");
  }
  if (numberEncryptedEntries > 1) {
    LOG_WARN(Stsd,
             "More than one encrypted sample description entry found while "
             "parsing track! We don't expect this, and it will likely break "
             "during fragment look up!");
  }
  LOG_DEBUG(Stsd,
            "Done, numberEncryptedEntries=%" PRIu32
            ", mSampleDescriptions.Length=%zu",
            numberEncryptedEntries, mSampleDescriptions.Length());
}

void MoofParser::ParseEncrypted(Box& aBox) {
  LOG_DEBUG(Moof, "Starting.");
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    // Some MP4 files have been found to have multiple sinf boxes in the same
    // enc* box. This does not match spec anyway, so just choose the first
    // one that parses properly.
    if (box.IsType("sinf")) {
      mSinf = Sinf(box);

      if (mSinf.IsValid()) {
        break;
      }
    }
  }
  LOG_DEBUG(Moof, "Done.");
}

class CtsComparator {
 public:
  bool Equals(Sample* const aA, Sample* const aB) const {
    return aA->mCompositionRange.start == aB->mCompositionRange.start;
  }
  bool LessThan(Sample* const aA, Sample* const aB) const {
    return aA->mCompositionRange.start < aB->mCompositionRange.start;
  }
};

Moof::Moof(Box& aBox, const TrackParseMode& aTrackParseMode, Trex& aTrex,
           Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, Sinf& aSinf,
           uint64_t* aDecodeTime, bool aIsAudio,
           nsTArray<TrackEndCts>& aTracksEndCts)
    : mRange(aBox.Range()),
      mTfhd(aTrex),
      // Do not reporting discontuities less than 35ms
      mMaxRoundingError(TimeUnit::FromSeconds(0.035)) {
  LOG_DEBUG(
      Moof,
      "Starting, aTrackParseMode=%s, track#=%" PRIu32
      " (ignore if multitrack).",
      aTrackParseMode.is<ParseAllTracks>() ? "multitrack" : "single track",
      aTrackParseMode.is<ParseAllTracks>() ? 0
                                           : aTrackParseMode.as<uint32_t>());
  MOZ_ASSERT(aTrackParseMode.is<ParseAllTracks>() ||
                 aTrex.mTrackId == aTrackParseMode.as<uint32_t>(),
             "If not parsing all tracks, aTrex should have the same track id "
             "as the track being parsed.");
  nsTArray<Box> psshBoxes;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("traf")) {
      ParseTraf(box, aTrackParseMode, aTrex, aMvhd, aMdhd, aEdts, aSinf,
                aDecodeTime, aIsAudio);
    }
    if (box.IsType("pssh")) {
      psshBoxes.AppendElement(box);
    }
  }

  // The EME spec requires that PSSH boxes which are contiguous in the
  // file are dispatched to the media element in a single "encrypted" event.
  // So append contiguous boxes here.
  for (size_t i = 0; i < psshBoxes.Length(); ++i) {
    Box box = psshBoxes[i];
    if (i == 0 || box.Offset() != psshBoxes[i - 1].NextOffset()) {
      mPsshes.AppendElement();
    }
    nsTArray<uint8_t>& pssh = mPsshes.LastElement();
    pssh.AppendElements(std::move(box.ReadCompleteBox()));
  }

  if (IsValid()) {
    if (mIndex.Length()) {
      // Ensure the samples are contiguous with no gaps.
      nsTArray<Sample*> ctsOrder;
      for (auto& sample : mIndex) {
        ctsOrder.AppendElement(&sample);
      }
      ctsOrder.Sort(CtsComparator());

      for (size_t i = 1; i < ctsOrder.Length(); i++) {
        ctsOrder[i - 1]->mCompositionRange.end =
            ctsOrder[i]->mCompositionRange.start;
      }

      // Ensure that there are no gaps between the first sample in this
      // Moof and the preceeding Moof.
      if (!ctsOrder.IsEmpty()) {
        bool found = false;
        // Track ID of the track we're parsing.
        const uint32_t trackId = aTrex.mTrackId;
        // Find the previous CTS end time of Moof preceeding the Moofs we just
        // parsed, for the track we're parsing.
        for (auto& prevCts : aTracksEndCts) {
          if (prevCts.mTrackId == trackId) {
            // We ensure there are no gaps in samples' CTS between the last
            // sample in a Moof, and the first sample in the next Moof, if
            // they're within these many Microseconds of each other.
            const TimeUnit CROSS_MOOF_CTS_MERGE_THRESHOLD =
                TimeUnit::FromMicroseconds(1);
            // We have previously parsed a Moof for this track. Smooth the gap
            // between samples for this track across the Moof bounary.
            if (ctsOrder[0]->mCompositionRange.start > prevCts.mCtsEndTime &&
                ctsOrder[0]->mCompositionRange.start - prevCts.mCtsEndTime <=
                    CROSS_MOOF_CTS_MERGE_THRESHOLD) {
              ctsOrder[0]->mCompositionRange.start = prevCts.mCtsEndTime;
            }
            prevCts.mCtsEndTime = ctsOrder.LastElement()->mCompositionRange.end;
            found = true;
            break;
          }
        }
        if (!found) {
          // We've not parsed a Moof for this track yet. Save its CTS end
          // time for the next Moof we parse.
          aTracksEndCts.AppendElement(TrackEndCts(
              trackId, ctsOrder.LastElement()->mCompositionRange.end));
        }
      }

      // In MP4, the duration of a sample is defined as the delta between two
      // decode timestamps. The operation above has updated the duration of each
      // sample as a Sample's duration is mCompositionRange.end -
      // mCompositionRange.start MSE's TrackBuffersManager expects dts that
      // increased by the sample's duration, so we rewrite the dts accordingly.
      TimeUnit presentationDuration =
          ctsOrder.LastElement()->mCompositionRange.end -
          ctsOrder[0]->mCompositionRange.start;
      auto decodeOffset =
          aMdhd.ToTimeUnit((int64_t)*aDecodeTime - aEdts.mMediaStart);
      auto offsetOffset = aMvhd.ToTimeUnit(aEdts.mEmptyOffset);
      TimeUnit endDecodeTime =
          (decodeOffset.isOk() && offsetOffset.isOk())
              ? decodeOffset.unwrap() + offsetOffset.unwrap()
              : TimeUnit::Zero(aMvhd.mTimescale);
      TimeUnit decodeDuration = endDecodeTime - mIndex[0].mDecodeTime;
      double adjust = 0.;
      if (!presentationDuration.IsZero()) {
        double num = decodeDuration.ToSeconds();
        double denom = presentationDuration.ToSeconds();
        if (denom != 0.) {
          adjust = num / denom;
        }
      }

      TimeUnit dtsOffset = mIndex[0].mDecodeTime;
      TimeUnit compositionDuration(0, aMvhd.mTimescale);
      // Adjust the dts, ensuring that the new adjusted dts will never be
      // greater than decodeTime (the next moof's decode start time).
      for (auto& sample : mIndex) {
        sample.mDecodeTime = dtsOffset + compositionDuration.MultDouble(adjust);
        compositionDuration += sample.mCompositionRange.Length();
      }
      mTimeRange =
          MP4Interval<TimeUnit>(ctsOrder[0]->mCompositionRange.start,
                                ctsOrder.LastElement()->mCompositionRange.end);
    }
    ProcessCencAuxInfo(aSinf.mDefaultEncryptionType);
  }
  LOG_DEBUG(Moof, "Done.");
}

bool Moof::GetAuxInfo(AtomType aType,
                      FallibleTArray<MediaByteRange>* aByteRanges) {
  LOG_DEBUG(Moof, "Starting.");
  aByteRanges->Clear();

  Saiz* saiz = nullptr;
  for (int i = 0;; i++) {
    if (i == mSaizs.Length()) {
      LOG_DEBUG(Moof, "Could not find saiz matching aType. Returning false.");
      return false;
    }
    if (mSaizs[i].mAuxInfoType == aType) {
      saiz = &mSaizs[i];
      break;
    }
  }
  Saio* saio = nullptr;
  for (int i = 0;; i++) {
    if (i == mSaios.Length()) {
      LOG_DEBUG(Moof, "Could not find saio matching aType. Returning false.");
      return false;
    }
    if (mSaios[i].mAuxInfoType == aType) {
      saio = &mSaios[i];
      break;
    }
  }

  if (saio->mOffsets.Length() == 1) {
    if (!aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length(),
                                  mozilla::fallible)) {
      LOG_ERROR(Moof, "OOM");
      return false;
    }
    uint64_t offset = mRange.mStart + saio->mOffsets[0];
    for (size_t i = 0; i < saiz->mSampleInfoSize.Length(); i++) {
      if (!aByteRanges->AppendElement(
              MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]),
              mozilla::fallible)) {
        LOG_ERROR(Moof, "OOM");
        return false;
      }
      offset += saiz->mSampleInfoSize[i];
    }
    LOG_DEBUG(
        Moof,
        "Saio has 1 entry. aByteRanges populated accordingly. Returning true.");
    return true;
  }

  if (saio->mOffsets.Length() == saiz->mSampleInfoSize.Length()) {
    if (!aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length(),
                                  mozilla::fallible)) {
      LOG_ERROR(Moof, "OOM");
      return false;
    }
    for (size_t i = 0; i < saio->mOffsets.Length(); i++) {
      uint64_t offset = mRange.mStart + saio->mOffsets[i];
      if (!aByteRanges->AppendElement(
              MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]),
              mozilla::fallible)) {
        LOG_ERROR(Moof, "OOM");
        return false;
      }
    }
    LOG_DEBUG(
        Moof,
        "Saio and saiz have same number of entries. aByteRanges populated "
        "accordingly. Returning true.");
    return true;
  }

  LOG_DEBUG(Moof,
            "Moof::GetAuxInfo could not find any Aux info, returning false.");
  return false;
}

bool Moof::ProcessCencAuxInfo(AtomType aScheme) {
  LOG_DEBUG(Moof, "Starting.");
  FallibleTArray<MediaByteRange> cencRanges;
  if (!GetAuxInfo(aScheme, &cencRanges) ||
      cencRanges.Length() != mIndex.Length()) {
    LOG_DEBUG(Moof, "Couldn't find cenc aux info.");
    return false;
  }
  for (int i = 0; i < cencRanges.Length(); i++) {
    mIndex[i].mCencRange = cencRanges[i];
  }
  LOG_DEBUG(Moof, "Found cenc aux info and stored on index.");
  return true;
}

void Moof::ParseTraf(Box& aBox, const TrackParseMode& aTrackParseMode,
                     Trex& aTrex, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts,
                     Sinf& aSinf, uint64_t* aDecodeTime, bool aIsAudio) {
  LOG_DEBUG(
      Traf,
      "Starting, aTrackParseMode=%s, track#=%" PRIu32
      " (ignore if multitrack).",
      aTrackParseMode.is<ParseAllTracks>() ? "multitrack" : "single track",
      aTrackParseMode.is<ParseAllTracks>() ? 0
                                           : aTrackParseMode.as<uint32_t>());
  MOZ_ASSERT(aDecodeTime);
  MOZ_ASSERT(aTrackParseMode.is<ParseAllTracks>() ||
                 aTrex.mTrackId == aTrackParseMode.as<uint32_t>(),
             "If not parsing all tracks, aTrex should have the same track id "
             "as the track being parsed.");
  Tfdt tfdt;

  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tfhd")) {
      mTfhd = Tfhd(box, aTrex);
    } else if (aTrackParseMode.is<ParseAllTracks>() ||
               mTfhd.mTrackId == aTrackParseMode.as<uint32_t>()) {
      if (box.IsType("tfdt")) {
        tfdt = Tfdt(box);
      } else if (box.IsType("sgpd")) {
        Sgpd sgpd(box);
        if (sgpd.IsValid() && sgpd.mGroupingType == "seig") {
          mFragmentSampleEncryptionInfoEntries.Clear();
          if (!mFragmentSampleEncryptionInfoEntries.AppendElements(
                  sgpd.mEntries, mozilla::fallible)) {
            LOG_ERROR(Moof, "OOM");
            return;
          }
        }
      } else if (box.IsType("sbgp")) {
        Sbgp sbgp(box);
        if (sbgp.IsValid() && sbgp.mGroupingType == "seig") {
          mFragmentSampleToGroupEntries.Clear();
          if (!mFragmentSampleToGroupEntries.AppendElements(
                  sbgp.mEntries, mozilla::fallible)) {
            LOG_ERROR(Moof, "OOM");
            return;
          }
        }
      } else if (box.IsType("saiz")) {
        if (!mSaizs.AppendElement(Saiz(box, aSinf.mDefaultEncryptionType),
                                  mozilla::fallible)) {
          LOG_ERROR(Moof, "OOM");
          return;
        }
      } else if (box.IsType("saio")) {
        if (!mSaios.AppendElement(Saio(box, aSinf.mDefaultEncryptionType),
                                  mozilla::fallible)) {
          LOG_ERROR(Moof, "OOM");
          return;
        }
      }
    }
  }
  if (aTrackParseMode.is<uint32_t>() &&
      mTfhd.mTrackId != aTrackParseMode.as<uint32_t>()) {
    LOG_DEBUG(Traf,
              "Early return as not multitrack parser and track id didn't match "
              "mTfhd.mTrackId=%" PRIu32,
              mTfhd.mTrackId);
    return;
  }
  // Now search for TRUN boxes.
  uint64_t decodeTime =
      tfdt.IsValid() ? tfdt.mBaseMediaDecodeTime : *aDecodeTime;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trun")) {
      if (ParseTrun(box, aMvhd, aMdhd, aEdts, &decodeTime, aIsAudio).isOk()) {
        mValid = true;
      } else {
        LOG_WARN(Moof, "ParseTrun failed");
        mValid = false;
        return;
      }
    }
  }
  *aDecodeTime = decodeTime;
  LOG_DEBUG(Traf, "Done, setting aDecodeTime=%." PRIu64 ".", decodeTime);
}

void Moof::FixRounding(const Moof& aMoof) {
  TimeUnit gap = aMoof.mTimeRange.start - mTimeRange.end;
  if (gap.IsPositive() && gap <= mMaxRoundingError) {
    mTimeRange.end = aMoof.mTimeRange.start;
  }
}

Result<Ok, nsresult> Moof::ParseTrun(Box& aBox, Mvhd& aMvhd, Mdhd& aMdhd,
                                     Edts& aEdts, uint64_t* aDecodeTime,
                                     bool aIsAudio) {
  LOG_DEBUG(Trun, "Starting.");
  if (!mTfhd.IsValid() || !aMvhd.IsValid() || !aMdhd.IsValid() ||
      !aEdts.IsValid()) {
    LOG_WARN(
        Moof, "Invalid dependencies: mTfhd(%d) aMvhd(%d) aMdhd(%d) aEdts(%d)",
        mTfhd.IsValid(), aMvhd.IsValid(), aMdhd.IsValid(), !aEdts.IsValid());
    return Err(NS_ERROR_FAILURE);
  }

  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG_WARN(Moof, "Incomplete Box (missing flags)");
    return Err(NS_ERROR_FAILURE);
  }
  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());

  if (!reader->CanReadType<uint32_t>()) {
    LOG_WARN(Moof, "Incomplete Box (missing sampleCount)");
    return Err(NS_ERROR_FAILURE);
  }
  uint32_t sampleCount;
  MOZ_TRY_VAR(sampleCount, reader->ReadU32());
  if (sampleCount == 0) {
    LOG_DEBUG(Trun, "Trun with no samples, returning.");
    return Ok();
  }

  uint64_t offset = mTfhd.mBaseDataOffset;
  if (flags & 0x01) {
    uint32_t tmp;
    MOZ_TRY_VAR(tmp, reader->ReadU32());
    offset += tmp;
  }
  uint32_t firstSampleFlags = mTfhd.mDefaultSampleFlags;
  if (flags & 0x04) {
    MOZ_TRY_VAR(firstSampleFlags, reader->ReadU32());
  }
  nsTArray<MP4Interval<TimeUnit>> timeRanges;
  uint64_t decodeTime = *aDecodeTime;

  if (!mIndex.SetCapacity(mIndex.Length() + sampleCount, fallible)) {
    LOG_ERROR(Moof, "Out of Memory");
    return Err(NS_ERROR_FAILURE);
  }

  for (size_t i = 0; i < sampleCount; i++) {
    uint32_t sampleDuration = mTfhd.mDefaultSampleDuration;
    if (flags & 0x100) {
      MOZ_TRY_VAR(sampleDuration, reader->ReadU32());
    }
    uint32_t sampleSize = mTfhd.mDefaultSampleSize;
    if (flags & 0x200) {
      MOZ_TRY_VAR(sampleSize, reader->ReadU32());
    }
    uint32_t sampleFlags = i ? mTfhd.mDefaultSampleFlags : firstSampleFlags;
    if (flags & 0x400) {
      MOZ_TRY_VAR(sampleFlags, reader->ReadU32());
    }
    int32_t ctsOffset = 0;
    if (flags & 0x800) {
      MOZ_TRY_VAR(ctsOffset, reader->Read32());
    }

    if (sampleSize) {
      Sample sample;
      sample.mByteRange = MediaByteRange(offset, offset + sampleSize);
      offset += sampleSize;

      TimeUnit decodeOffset, emptyOffset, startCts, endCts;
      MOZ_TRY_VAR(decodeOffset,
                  aMdhd.ToTimeUnit((int64_t)decodeTime - aEdts.mMediaStart));
      MOZ_TRY_VAR(emptyOffset, aMvhd.ToTimeUnit(aEdts.mEmptyOffset));
      sample.mDecodeTime = decodeOffset + emptyOffset;
      MOZ_TRY_VAR(startCts, aMdhd.ToTimeUnit((int64_t)decodeTime + ctsOffset -
                                             aEdts.mMediaStart));
      MOZ_TRY_VAR(endCts, aMdhd.ToTimeUnit((int64_t)decodeTime + ctsOffset +
                                           sampleDuration - aEdts.mMediaStart));
      sample.mCompositionRange =
          MP4Interval<TimeUnit>(startCts + emptyOffset, endCts + emptyOffset);
      // Sometimes audio streams don't properly mark their samples as keyframes,
      // because every audio sample is a keyframe.
      sample.mSync = !(sampleFlags & 0x1010000) || aIsAudio;

      MOZ_ALWAYS_TRUE(mIndex.AppendElement(sample, fallible));

      mMdatRange = mMdatRange.Span(sample.mByteRange);
    }
    decodeTime += sampleDuration;
  }
  TimeUnit roundTime;
  MOZ_TRY_VAR(roundTime, aMdhd.ToTimeUnit(sampleCount));
  mMaxRoundingError = roundTime + mMaxRoundingError;

  *aDecodeTime = decodeTime;

  LOG_DEBUG(Trun, "Done.");
  return Ok();
}

Tkhd::Tkhd(Box& aBox) : mTrackId(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Tkhd, "Parse failed");
  }
}

Result<Ok, nsresult> Tkhd::Parse(Box& aBox) {
  BoxReader reader(aBox);
  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;
  if (version == 0) {
    uint32_t creationTime, modificationTime, reserved, duration;
    MOZ_TRY_VAR(creationTime, reader->ReadU32());
    MOZ_TRY_VAR(modificationTime, reader->ReadU32());
    MOZ_TRY_VAR(mTrackId, reader->ReadU32());
    MOZ_TRY_VAR(reserved, reader->ReadU32());
    MOZ_TRY_VAR(duration, reader->ReadU32());

    (void)reserved;
    NS_ASSERTION(!reserved, "reserved should be 0");

    mCreationTime = creationTime;
    mModificationTime = modificationTime;
    mDuration = duration;
  } else if (version == 1) {
    uint32_t reserved;
    MOZ_TRY_VAR(mCreationTime, reader->ReadU64());
    MOZ_TRY_VAR(mModificationTime, reader->ReadU64());
    MOZ_TRY_VAR(mTrackId, reader->ReadU32());
    MOZ_TRY_VAR(reserved, reader->ReadU32());
    (void)reserved;
    NS_ASSERTION(!reserved, "reserved should be 0");
    MOZ_TRY_VAR(mDuration, reader->ReadU64());
  }
  return Ok();
}

Mvhd::Mvhd(Box& aBox)
    : mCreationTime(0), mModificationTime(0), mTimescale(0), mDuration(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Mvhd, "Parse failed");
  }
}

Result<Ok, nsresult> Mvhd::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;

  if (version == 0) {
    uint32_t creationTime, modificationTime, duration;
    MOZ_TRY_VAR(creationTime, reader->ReadU32());
    MOZ_TRY_VAR(modificationTime, reader->ReadU32());
    MOZ_TRY_VAR(mTimescale, reader->ReadU32());
    MOZ_TRY_VAR(duration, reader->ReadU32());
    mCreationTime = creationTime;
    mModificationTime = modificationTime;
    mDuration = duration;
  } else if (version == 1) {
    MOZ_TRY_VAR(mCreationTime, reader->ReadU64());
    MOZ_TRY_VAR(mModificationTime, reader->ReadU64());
    MOZ_TRY_VAR(mTimescale, reader->ReadU32());
    MOZ_TRY_VAR(mDuration, reader->ReadU64());
  } else {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

Mdhd::Mdhd(Box& aBox) : Mvhd(aBox) {}

Trex::Trex(Box& aBox)
    : mFlags(0),
      mTrackId(0),
      mDefaultSampleDescriptionIndex(0),
      mDefaultSampleDuration(0),
      mDefaultSampleSize(0),
      mDefaultSampleFlags(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Trex, "Parse failed");
  }
}

Result<Ok, nsresult> Trex::Parse(Box& aBox) {
  BoxReader reader(aBox);

  MOZ_TRY_VAR(mFlags, reader->ReadU32());
  MOZ_TRY_VAR(mTrackId, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleDescriptionIndex, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleDuration, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleSize, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleFlags, reader->ReadU32());

  return Ok();
}

Tfhd::Tfhd(Box& aBox, Trex& aTrex) : Trex(aTrex), mBaseDataOffset(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Tfhd, "Parse failed");
  }
}

Result<Ok, nsresult> Tfhd::Parse(Box& aBox) {
  MOZ_ASSERT(aBox.IsType("tfhd"));
  MOZ_ASSERT(aBox.Parent()->IsType("traf"));
  MOZ_ASSERT(aBox.Parent()->Parent()->IsType("moof"));

  BoxReader reader(aBox);

  MOZ_TRY_VAR(mFlags, reader->ReadU32());
  MOZ_TRY_VAR(mTrackId, reader->ReadU32());
  mBaseDataOffset = aBox.Parent()->Parent()->Offset();
  if (mFlags & 0x01) {
    MOZ_TRY_VAR(mBaseDataOffset, reader->ReadU64());
  }
  if (mFlags & 0x02) {
    MOZ_TRY_VAR(mDefaultSampleDescriptionIndex, reader->ReadU32());
  }
  if (mFlags & 0x08) {
    MOZ_TRY_VAR(mDefaultSampleDuration, reader->ReadU32());
  }
  if (mFlags & 0x10) {
    MOZ_TRY_VAR(mDefaultSampleSize, reader->ReadU32());
  }
  if (mFlags & 0x20) {
    MOZ_TRY_VAR(mDefaultSampleFlags, reader->ReadU32());
  }

  return Ok();
}

Tfdt::Tfdt(Box& aBox) : mBaseMediaDecodeTime(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Tfdt, "Parse failed");
  }
}

Result<Ok, nsresult> Tfdt::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;
  if (version == 0) {
    uint32_t tmp;
    MOZ_TRY_VAR(tmp, reader->ReadU32());
    mBaseMediaDecodeTime = tmp;
  } else if (version == 1) {
    MOZ_TRY_VAR(mBaseMediaDecodeTime, reader->ReadU64());
  }
  return Ok();
}

Edts::Edts(Box& aBox) : mMediaStart(0), mEmptyOffset(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Edts, "Parse failed");
  }
}

Result<Ok, nsresult> Edts::Parse(Box& aBox) {
  Box child = aBox.FirstChild();
  if (!child.IsType("elst")) {
    return Err(NS_ERROR_FAILURE);
  }

  BoxReader reader(child);
  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;
  bool emptyEntry = false;
  uint32_t entryCount;
  MOZ_TRY_VAR(entryCount, reader->ReadU32());
  for (uint32_t i = 0; i < entryCount; i++) {
    uint64_t segment_duration;
    int64_t media_time;
    if (version == 1) {
      MOZ_TRY_VAR(segment_duration, reader->ReadU64());
      MOZ_TRY_VAR(media_time, reader->Read64());
    } else {
      uint32_t tmp;
      MOZ_TRY_VAR(tmp, reader->ReadU32());
      segment_duration = tmp;
      int32_t tmp2;
      MOZ_TRY_VAR(tmp2, reader->Read32());
      media_time = tmp2;
    }
    if (media_time == -1 && i) {
      LOG_WARN(Edts, "Multiple empty edit, not handled");
    } else if (media_time == -1) {
      if (segment_duration > std::numeric_limits<int64_t>::max()) {
        NS_WARNING("Segment duration higher than int64_t max.");
        mEmptyOffset = std::numeric_limits<int64_t>::max();
      } else {
        mEmptyOffset = static_cast<int64_t>(segment_duration);
      }
      emptyEntry = true;
    } else if (i > 1 || (i > 0 && !emptyEntry)) {
      LOG_WARN(Edts,
               "More than one edit entry, not handled. A/V sync will be wrong");
      break;
    } else {
      mMediaStart = media_time;
    }
    MOZ_TRY(reader->ReadU32());  // media_rate_integer and media_rate_fraction
  }

  return Ok();
}

Saiz::Saiz(Box& aBox, AtomType aDefaultType)
    : mAuxInfoType(aDefaultType), mAuxInfoTypeParameter(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Saiz, "Parse failed");
  }
}

Result<Ok, nsresult> Saiz::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  if (flags & 1) {
    MOZ_TRY_VAR(mAuxInfoType, reader->ReadU32());
    MOZ_TRY_VAR(mAuxInfoTypeParameter, reader->ReadU32());
  }
  uint8_t defaultSampleInfoSize;
  MOZ_TRY_VAR(defaultSampleInfoSize, reader->ReadU8());
  uint32_t count;
  MOZ_TRY_VAR(count, reader->ReadU32());
  if (defaultSampleInfoSize) {
    if (!mSampleInfoSize.SetLength(count, fallible)) {
      LOG_ERROR(Saiz, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
    memset(mSampleInfoSize.Elements(), defaultSampleInfoSize,
           mSampleInfoSize.Length());
  } else {
    if (!reader->ReadArray(mSampleInfoSize, count)) {
      LOG_WARN(Saiz, "Incomplete Box (OOM or missing count:%u)", count);
      return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Saio::Saio(Box& aBox, AtomType aDefaultType)
    : mAuxInfoType(aDefaultType), mAuxInfoTypeParameter(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Saio, "Parse failed");
  }
}

Result<Ok, nsresult> Saio::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;
  if (flags & 1) {
    MOZ_TRY_VAR(mAuxInfoType, reader->ReadU32());
    MOZ_TRY_VAR(mAuxInfoTypeParameter, reader->ReadU32());
  }

  size_t count;
  MOZ_TRY_VAR(count, reader->ReadU32());
  if (!mOffsets.SetCapacity(count, fallible)) {
    LOG_ERROR(Saiz, "OOM");
    return Err(NS_ERROR_FAILURE);
  }
  if (version == 0) {
    uint32_t offset;
    for (size_t i = 0; i < count; i++) {
      MOZ_TRY_VAR(offset, reader->ReadU32());
      MOZ_ALWAYS_TRUE(mOffsets.AppendElement(offset, fallible));
    }
  } else {
    uint64_t offset;
    for (size_t i = 0; i < count; i++) {
      MOZ_TRY_VAR(offset, reader->ReadU64());
      MOZ_ALWAYS_TRUE(mOffsets.AppendElement(offset, fallible));
    }
  }
  return Ok();
}

Sbgp::Sbgp(Box& aBox) : mGroupingTypeParam(0) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Sbgp, "Parse failed");
  }
}

Result<Ok, nsresult> Sbgp::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  const uint8_t version = flags >> 24;

  uint32_t type;
  MOZ_TRY_VAR(type, reader->ReadU32());
  mGroupingType = type;

  if (version == 1) {
    MOZ_TRY_VAR(mGroupingTypeParam, reader->ReadU32());
  }

  uint32_t count;
  MOZ_TRY_VAR(count, reader->ReadU32());

  for (uint32_t i = 0; i < count; i++) {
    uint32_t sampleCount;
    MOZ_TRY_VAR(sampleCount, reader->ReadU32());
    uint32_t groupDescriptionIndex;
    MOZ_TRY_VAR(groupDescriptionIndex, reader->ReadU32());

    SampleToGroupEntry entry(sampleCount, groupDescriptionIndex);
    if (!mEntries.AppendElement(entry, mozilla::fallible)) {
      LOG_ERROR(Sbgp, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Sgpd::Sgpd(Box& aBox) {
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG_WARN(Sgpd, "Parse failed");
  }
}

Result<Ok, nsresult> Sgpd::Parse(Box& aBox) {
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  const uint8_t version = flags >> 24;

  uint32_t type;
  MOZ_TRY_VAR(type, reader->ReadU32());
  mGroupingType = type;

  const uint32_t entrySize = sizeof(uint32_t) + kKeyIdSize;
  uint32_t defaultLength = 0;

  if (version == 1) {
    MOZ_TRY_VAR(defaultLength, reader->ReadU32());
    if (defaultLength < entrySize && defaultLength != 0) {
      return Err(NS_ERROR_FAILURE);
    }
  }

  uint32_t count;
  MOZ_TRY_VAR(count, reader->ReadU32());

  for (uint32_t i = 0; i < count; ++i) {
    if (version == 1 && defaultLength == 0) {
      uint32_t descriptionLength;
      MOZ_TRY_VAR(descriptionLength, reader->ReadU32());
      if (descriptionLength < entrySize) {
        return Err(NS_ERROR_FAILURE);
      }
    }

    CencSampleEncryptionInfoEntry entry;
    bool valid = entry.Init(reader).isOk();
    if (!valid) {
      return Err(NS_ERROR_FAILURE);
    }
    if (!mEntries.AppendElement(entry, mozilla::fallible)) {
      LOG_ERROR(Sgpd, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Result<Ok, nsresult> CencSampleEncryptionInfoEntry::Init(BoxReader& aReader) {
  // Skip a reserved byte.
  MOZ_TRY(aReader->ReadU8());

  uint8_t pattern;
  MOZ_TRY_VAR(pattern, aReader->ReadU8());
  mCryptByteBlock = pattern >> 4;
  mSkipByteBlock = pattern & 0x0f;

  uint8_t isEncrypted;
  MOZ_TRY_VAR(isEncrypted, aReader->ReadU8());
  mIsEncrypted = isEncrypted != 0;

  MOZ_TRY_VAR(mIVSize, aReader->ReadU8());

  // Read the key id.
  if (!mKeyId.SetLength(kKeyIdSize, fallible)) {
    LOG_ERROR(CencSampleEncryptionInfoEntry, "OOM");
    return Err(NS_ERROR_FAILURE);
  }
  for (uint32_t i = 0; i < kKeyIdSize; ++i) {
    MOZ_TRY_VAR(mKeyId.ElementAt(i), aReader->ReadU8());
  }

  if (mIsEncrypted) {
    if (mIVSize != 8 && mIVSize != 16) {
      return Err(NS_ERROR_FAILURE);
    }
  } else if (mIVSize != 0) {
    // Protected content with 0 sized IV indicates a constant IV is present.
    // This is used for the cbcs scheme.
    uint8_t constantIVSize;
    MOZ_TRY_VAR(constantIVSize, aReader->ReadU8());
    if (constantIVSize != 8 && constantIVSize != 16) {
      LOG_WARN(CencSampleEncryptionInfoEntry,
               "Unexpected constantIVSize: %" PRIu8, constantIVSize);
      return Err(NS_ERROR_FAILURE);
    }
    if (!mConsantIV.SetLength(constantIVSize, mozilla::fallible)) {
      LOG_ERROR(CencSampleEncryptionInfoEntry, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
    for (uint32_t i = 0; i < constantIVSize; ++i) {
      MOZ_TRY_VAR(mConsantIV.ElementAt(i), aReader->ReadU8());
    }
  }

  return Ok();
}
}  // namespace mozilla

#undef LOG_DEBUG
#undef LOG_WARN
#undef LOG_ERROR
