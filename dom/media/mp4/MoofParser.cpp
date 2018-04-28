/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoofParser.h"
#include "Box.h"
#include "SinfParser.h"
#include <limits>
#include "MP4Interval.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Logging.h"

#if defined(MOZ_FMP4)
extern mozilla::LogModule* GetDemuxerLog();

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOG(name, arg, ...) MOZ_LOG(GetDemuxerLog(), mozilla::LogLevel::Debug, (TOSTRING(name) "(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla
{

const uint32_t kKeyIdSize = 16;

bool
MoofParser::RebuildFragmentedIndex(const MediaByteRangeSet& aByteRanges)
{
  BoxContext context(mSource, aByteRanges);
  return RebuildFragmentedIndex(context);
}

bool
MoofParser::RebuildFragmentedIndex(
  const MediaByteRangeSet& aByteRanges, bool* aCanEvict)
{
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

bool
MoofParser::RebuildFragmentedIndex(BoxContext& aContext)
{
  bool foundValidMoof = false;
  bool foundMdat = false;

  for (Box box(&aContext, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moov") && mInitRange.IsEmpty()) {
      mInitRange = MediaByteRange(0, box.Range().mEnd);
      ParseMoov(box);
    } else if (box.IsType("moof")) {
      Moof moof(box, mTrex, mMvhd, mMdhd, mEdts, mSinf, &mLastDecodeTime, mIsAudio);

      if (!moof.IsValid() && !box.Next().IsAvailable()) {
        // Moof isn't valid abort search for now.
        break;
      }

      if (!mMoofs.IsEmpty()) {
        // Stitch time ranges together in the case of a (hopefully small) time
        // range gap between moofs.
        mMoofs.LastElement().FixRounding(moof);
      }

      mMoofs.AppendElement(moof);
      mMediaRanges.AppendElement(moof.mRange);
      foundValidMoof = true;
    } else if (box.IsType("mdat") && !Moofs().IsEmpty()) {
      // Check if we have all our data from last moof.
      Moof& moof = Moofs().LastElement();
      media::Interval<int64_t> datarange(moof.mMdatRange.mStart, moof.mMdatRange.mEnd, 0);
      media::Interval<int64_t> mdat(box.Range().mStart, box.Range().mEnd, 0);
      if (datarange.Intersects(mdat)) {
        mMediaRanges.LastElement() =
          mMediaRanges.LastElement().Span(box.Range());
      }
    }
    mOffset = box.NextOffset();
  }
  return foundValidMoof;
}

MediaByteRange
MoofParser::FirstCompleteMediaHeader()
{
  if (Moofs().IsEmpty()) {
    return MediaByteRange();
  }
  return Moofs()[0].mRange;
}

MediaByteRange
MoofParser::FirstCompleteMediaSegment()
{
  for (uint32_t i = 0 ; i < mMediaRanges.Length(); i++) {
    if (mMediaRanges[i].Contains(Moofs()[i].mMdatRange)) {
      return mMediaRanges[i];
    }
  }
  return MediaByteRange();
}

DDLoggedTypeDeclNameAndBase(BlockingStream, ByteStream);

class BlockingStream
  : public ByteStream
  , public DecoderDoctorLifeLogger<BlockingStream>
{
public:
  explicit BlockingStream(ByteStream* aStream) : mStream(aStream)
  {
    DDLINKCHILD("stream", aStream);
  }

  bool ReadAt(int64_t offset, void* data, size_t size, size_t* bytes_read)
    override
  {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  bool CachedReadAt(int64_t offset, void* data, size_t size, size_t* bytes_read)
    override
  {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  virtual bool Length(int64_t* size) override
  {
    return mStream->Length(size);
  }

private:
  RefPtr<ByteStream> mStream;
};

bool
MoofParser::BlockingReadNextMoof()
{
  int64_t length = std::numeric_limits<int64_t>::max();
  mSource->Length(&length);
  MediaByteRangeSet byteRanges;
  byteRanges += MediaByteRange(0, length);
  RefPtr<BlockingStream> stream = new BlockingStream(mSource);

  BoxContext context(stream, byteRanges);
  for (Box box(&context, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moof")) {
      byteRanges.Clear();
      byteRanges += MediaByteRange(mOffset, box.Range().mEnd);
      return RebuildFragmentedIndex(context);
    }
  }
  return false;
}

void
MoofParser::ScanForMetadata(mozilla::MediaByteRange& aMoov)
{
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
}

already_AddRefed<mozilla::MediaByteBuffer>
MoofParser::Metadata()
{
  MediaByteRange moov;
  ScanForMetadata(moov);
  CheckedInt<MediaByteBuffer::size_type> moovLength = moov.Length();
  if (!moovLength.isValid() || !moovLength.value()) {
    // No moov, or cannot be used as array size.
    return nullptr;
  }

  RefPtr<MediaByteBuffer> metadata = new MediaByteBuffer();
  if (!metadata->SetLength(moovLength.value(), fallible)) {
    LOG(Moof, "OOM");
    return nullptr;
  }

  RefPtr<BlockingStream> stream = new BlockingStream(mSource);
  size_t read;
  bool rv =
    stream->ReadAt(moov.mStart, metadata->Elements(), moovLength.value(), &read);
  if (!rv || read != moovLength.value()) {
    return nullptr;
  }
  return metadata.forget();
}

MP4Interval<Microseconds>
MoofParser::GetCompositionRange(const MediaByteRangeSet& aByteRanges)
{
  MP4Interval<Microseconds> compositionRange;
  BoxContext context(mSource, aByteRanges);
  for (size_t i = 0; i < mMoofs.Length(); i++) {
    Moof& moof = mMoofs[i];
    Box box(&context, moof.mRange.mStart);
    if (box.IsAvailable()) {
      compositionRange = compositionRange.Extents(moof.mTimeRange);
    }
  }
  return compositionRange;
}

bool
MoofParser::ReachedEnd()
{
  int64_t length;
  return mSource->Length(&length) && mOffset == length;
}

void
MoofParser::ParseMoov(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mvhd")) {
      mMvhd = Mvhd(box);
    } else if (box.IsType("trak")) {
      ParseTrak(box);
    } else if (box.IsType("mvex")) {
      ParseMvex(box);
    }
  }
}

void
MoofParser::ParseTrak(Box& aBox)
{
  Tkhd tkhd;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tkhd")) {
      tkhd = Tkhd(box);
    } else if (box.IsType("mdia")) {
      if (!mTrex.mTrackId || tkhd.mTrackId == mTrex.mTrackId) {
        ParseMdia(box, tkhd);
      }
    } else if (box.IsType("edts") &&
               (!mTrex.mTrackId || tkhd.mTrackId == mTrex.mTrackId)) {
      mEdts = Edts(box);
    }
  }
}

void
MoofParser::ParseMdia(Box& aBox, Tkhd& aTkhd)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mdhd")) {
      mMdhd = Mdhd(box);
    } else if (box.IsType("minf")) {
      ParseMinf(box);
    }
  }
}

void
MoofParser::ParseMvex(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trex")) {
      Trex trex = Trex(box);
      if (!mTrex.mTrackId || trex.mTrackId == mTrex.mTrackId) {
        auto trackId = mTrex.mTrackId;
        mTrex = trex;
        // Keep the original trackId, as should it be 0 we want to continue
        // parsing all tracks.
        mTrex.mTrackId = trackId;
      }
    }
  }
}

void
MoofParser::ParseMinf(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stbl")) {
      ParseStbl(box);
    }
  }
}

void
MoofParser::ParseStbl(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stsd")) {
      ParseStsd(box);
    } else if (box.IsType("sgpd")) {
      Sgpd sgpd(box);
      if (sgpd.IsValid() && sgpd.mGroupingType == "seig") {
        mTrackSampleEncryptionInfoEntries.Clear();
        if (!mTrackSampleEncryptionInfoEntries.AppendElements(sgpd.mEntries, mozilla::fallible)) {
          LOG(Moof, "OOM");
          return;
        }
      }
    } else if (box.IsType("sbgp")) {
      Sbgp sbgp(box);
      if (sbgp.IsValid() && sbgp.mGroupingType == "seig") {
        mTrackSampleToGroupEntries.Clear();
        if (!mTrackSampleToGroupEntries.AppendElements(sbgp.mEntries, mozilla::fallible)) {
          LOG(Moof, "OOM");
          return;
        }
      }
    }
  }
}

void
MoofParser::ParseStsd(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("encv") || box.IsType("enca")) {
      ParseEncrypted(box);
    }
  }
}

void
MoofParser::ParseEncrypted(Box& aBox)
{
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
}

class CtsComparator
{
public:
  bool Equals(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start == aB->mCompositionRange.start;
  }
  bool
  LessThan(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start < aB->mCompositionRange.start;
  }
};

Moof::Moof(Box& aBox, Trex& aTrex, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, Sinf& aSinf, uint64_t* aDecodeTime, bool aIsAudio)
  : mRange(aBox.Range())
  , mMaxRoundingError(35000)
{
  nsTArray<Box> psshBoxes;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("traf")) {
      ParseTraf(box, aTrex, aMvhd, aMdhd, aEdts, aSinf, aDecodeTime, aIsAudio);
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
    pssh.AppendElements(box.Header());
    pssh.AppendElements(box.Read());
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
        ctsOrder[i-1]->mCompositionRange.end = ctsOrder[i]->mCompositionRange.start;
      }

      // In MP4, the duration of a sample is defined as the delta between two decode
      // timestamps. The operation above has updated the duration of each sample
      // as a Sample's duration is mCompositionRange.end - mCompositionRange.start
      // MSE's TrackBuffersManager expects dts that increased by the sample's
      // duration, so we rewrite the dts accordingly.
      int64_t presentationDuration =
        ctsOrder.LastElement()->mCompositionRange.end
        - ctsOrder[0]->mCompositionRange.start;
      auto decodeOffset = aMdhd.ToMicroseconds((int64_t)*aDecodeTime - aEdts.mMediaStart);
      auto offsetOffset = aMvhd.ToMicroseconds(aEdts.mEmptyOffset);
      int64_t endDecodeTime = decodeOffset.isOk() & offsetOffset.isOk() ?
                              decodeOffset.unwrap() + offsetOffset.unwrap() : 0;
      int64_t decodeDuration = endDecodeTime - mIndex[0].mDecodeTime;
      double adjust = !!presentationDuration ? (double)decodeDuration / presentationDuration : 0;
      int64_t dtsOffset = mIndex[0].mDecodeTime;
      int64_t compositionDuration = 0;
      // Adjust the dts, ensuring that the new adjusted dts will never be greater
      // than decodeTime (the next moof's decode start time).
      for (auto& sample : mIndex) {
        sample.mDecodeTime = dtsOffset + int64_t(compositionDuration * adjust);
        compositionDuration += sample.mCompositionRange.Length();
      }
      mTimeRange = MP4Interval<Microseconds>(ctsOrder[0]->mCompositionRange.start,
          ctsOrder.LastElement()->mCompositionRange.end);
    }
    ProcessCenc();
  }
}

bool
Moof::GetAuxInfo(AtomType aType, FallibleTArray<MediaByteRange>* aByteRanges)
{
  aByteRanges->Clear();

  Saiz* saiz = nullptr;
  for (int i = 0; ; i++) {
    if (i == mSaizs.Length()) {
      return false;
    }
    if (mSaizs[i].mAuxInfoType == aType) {
      saiz = &mSaizs[i];
      break;
    }
  }
  Saio* saio = nullptr;
  for (int i = 0; ; i++) {
    if (i == mSaios.Length()) {
      return false;
    }
    if (mSaios[i].mAuxInfoType == aType) {
      saio = &mSaios[i];
      break;
    }
  }

  if (saio->mOffsets.Length() == 1) {
    if (!aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length(), mozilla::fallible)) {
      LOG(Moof, "OOM");
      return false;
    }
    uint64_t offset = mRange.mStart + saio->mOffsets[0];
    for (size_t i = 0; i < saiz->mSampleInfoSize.Length(); i++) {
      if (!aByteRanges->AppendElement(
           MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]), mozilla::fallible)) {
        LOG(Moof, "OOM");
        return false;
      }
      offset += saiz->mSampleInfoSize[i];
    }
    return true;
  }

  if (saio->mOffsets.Length() == saiz->mSampleInfoSize.Length()) {
    if (!aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length(), mozilla::fallible)) {
      LOG(Moof, "OOM");
      return false;
    }
    for (size_t i = 0; i < saio->mOffsets.Length(); i++) {
      uint64_t offset = mRange.mStart + saio->mOffsets[i];
      if (!aByteRanges->AppendElement(
            MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]), mozilla::fallible)) {
        LOG(Moof, "OOM");
        return false;
      }
    }
    return true;
  }

  return false;
}

bool
Moof::ProcessCenc()
{
  FallibleTArray<MediaByteRange> cencRanges;
  if (!GetAuxInfo(AtomType("cenc"), &cencRanges) ||
      cencRanges.Length() != mIndex.Length()) {
    return false;
  }
  for (int i = 0; i < cencRanges.Length(); i++) {
    mIndex[i].mCencRange = cencRanges[i];
  }
  return true;
}

void
Moof::ParseTraf(Box& aBox, Trex& aTrex, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, Sinf& aSinf, uint64_t* aDecodeTime, bool aIsAudio)
{
  MOZ_ASSERT(aDecodeTime);
  Tfhd tfhd(aTrex);
  Tfdt tfdt;

  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tfhd")) {
      tfhd = Tfhd(box, aTrex);
    } else if (!aTrex.mTrackId || tfhd.mTrackId == aTrex.mTrackId) {
      if (box.IsType("tfdt")) {
        tfdt = Tfdt(box);
      } else if (box.IsType("sgpd")) {
        Sgpd sgpd(box);
        if (sgpd.IsValid() && sgpd.mGroupingType == "seig") {
          mFragmentSampleEncryptionInfoEntries.Clear();
          if (!mFragmentSampleEncryptionInfoEntries.AppendElements(sgpd.mEntries, mozilla::fallible)) {
            LOG(Moof, "OOM");
            return;
          }
        }
      } else if (box.IsType("sbgp")) {
        Sbgp sbgp(box);
        if (sbgp.IsValid() && sbgp.mGroupingType == "seig") {
          mFragmentSampleToGroupEntries.Clear();
          if (!mFragmentSampleToGroupEntries.AppendElements(sbgp.mEntries, mozilla::fallible)) {
            LOG(Moof, "OOM");
            return;
          }
        }
      } else if (box.IsType("saiz")) {
        if (!mSaizs.AppendElement(Saiz(box, aSinf.mDefaultEncryptionType), mozilla::fallible)) {
          LOG(Moof, "OOM");
          return;
        }
      } else if (box.IsType("saio")) {
        if (!mSaios.AppendElement(Saio(box, aSinf.mDefaultEncryptionType), mozilla::fallible)) {
          LOG(Moof, "OOM");
          return;
        }
      }
    }
  }
  if (aTrex.mTrackId && tfhd.mTrackId != aTrex.mTrackId) {
    return;
  }
  // Now search for TRUN boxes.
  uint64_t decodeTime =
    tfdt.IsValid() ? tfdt.mBaseMediaDecodeTime : *aDecodeTime;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trun")) {
      if (ParseTrun(box, tfhd, aMvhd, aMdhd, aEdts, &decodeTime, aIsAudio).isOk()) {
        mValid = true;
      } else {
        LOG(Moof, "ParseTrun failed");
        mValid = false;
        break;
      }
    }
  }
  *aDecodeTime = decodeTime;
}

void
Moof::FixRounding(const Moof& aMoof) {
  Microseconds gap = aMoof.mTimeRange.start - mTimeRange.end;
  if (gap > 0 && gap <= mMaxRoundingError) {
    mTimeRange.end = aMoof.mTimeRange.start;
  }
}

Result<Ok, nsresult>
Moof::ParseTrun(Box& aBox, Tfhd& aTfhd, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, uint64_t* aDecodeTime, bool aIsAudio)
{
  if (!aTfhd.IsValid() || !aMvhd.IsValid() || !aMdhd.IsValid() ||
      !aEdts.IsValid()) {
    LOG(Moof, "Invalid dependencies: aTfhd(%d) aMvhd(%d) aMdhd(%d) aEdts(%d)",
        aTfhd.IsValid(), aMvhd.IsValid(), aMdhd.IsValid(), !aEdts.IsValid());
    return Err(NS_ERROR_FAILURE);
  }

  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Moof, "Incomplete Box (missing flags)");
    return Err(NS_ERROR_FAILURE);
  }
  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  uint8_t version = flags >> 24;

  if (!reader->CanReadType<uint32_t>()) {
    LOG(Moof, "Incomplete Box (missing sampleCount)");
    return Err(NS_ERROR_FAILURE);
  }
  uint32_t sampleCount;
  MOZ_TRY_VAR(sampleCount, reader->ReadU32());
  if (sampleCount == 0) {
    return Ok();
  }

  uint64_t offset = aTfhd.mBaseDataOffset;
  if (flags & 0x01) {
    uint32_t tmp;
    MOZ_TRY_VAR(tmp, reader->ReadU32());
    offset += tmp;
  }
  uint32_t firstSampleFlags = aTfhd.mDefaultSampleFlags;
  if (flags & 0x04) {
    MOZ_TRY_VAR(firstSampleFlags, reader->ReadU32());
  }
  uint64_t decodeTime = *aDecodeTime;
  nsTArray<MP4Interval<Microseconds>> timeRanges;

  if (!mIndex.SetCapacity(sampleCount, fallible)) {
    LOG(Moof, "Out of Memory");
    return Err(NS_ERROR_FAILURE);
  }

  for (size_t i = 0; i < sampleCount; i++) {
    uint32_t sampleDuration = aTfhd.mDefaultSampleDuration;
    if (flags & 0x100) {
      MOZ_TRY_VAR(sampleDuration, reader->ReadU32());
    }
    uint32_t sampleSize = aTfhd.mDefaultSampleSize;
    if (flags & 0x200) {
      MOZ_TRY_VAR(sampleSize, reader->ReadU32());
    }
    uint32_t sampleFlags = i ? aTfhd.mDefaultSampleFlags : firstSampleFlags;
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

      Microseconds decodeOffset, emptyOffset, startCts, endCts;
      MOZ_TRY_VAR(decodeOffset, aMdhd.ToMicroseconds((int64_t)decodeTime - aEdts.mMediaStart));
      MOZ_TRY_VAR(emptyOffset, aMvhd.ToMicroseconds(aEdts.mEmptyOffset));
      sample.mDecodeTime = decodeOffset + emptyOffset;
      MOZ_TRY_VAR(startCts, aMdhd.ToMicroseconds((int64_t)decodeTime + ctsOffset - aEdts.mMediaStart));
      MOZ_TRY_VAR(endCts, aMdhd.ToMicroseconds((int64_t)decodeTime + ctsOffset + sampleDuration - aEdts.mMediaStart));
      sample.mCompositionRange = MP4Interval<Microseconds>(startCts + emptyOffset, endCts + emptyOffset);
      // Sometimes audio streams don't properly mark their samples as keyframes,
      // because every audio sample is a keyframe.
      sample.mSync = !(sampleFlags & 0x1010000) || aIsAudio;

      // FIXME: Make this infallible after bug 968520 is done.
      MOZ_ALWAYS_TRUE(mIndex.AppendElement(sample, fallible));

      mMdatRange = mMdatRange.Span(sample.mByteRange);
    }
    decodeTime += sampleDuration;
  }
  Microseconds roundTime;
  MOZ_TRY_VAR(roundTime, aMdhd.ToMicroseconds(sampleCount));
  mMaxRoundingError += roundTime;

  *aDecodeTime = decodeTime;

  return Ok();
}

Tkhd::Tkhd(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Tkhd, "Parse failed");
  }
}

Result<Ok, nsresult>
Tkhd::Parse(Box& aBox)
{
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
    NS_ASSERTION(!reserved, "reserved should be 0");
    MOZ_TRY_VAR(mDuration, reader->ReadU64());
  }
  return Ok();
}

Mvhd::Mvhd(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Mvhd, "Parse failed");
  }
}

Result<Ok, nsresult>
Mvhd::Parse(Box& aBox)
{
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

Mdhd::Mdhd(Box& aBox)
  : Mvhd(aBox)
{
}

Trex::Trex(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Trex, "Parse failed");
  }
}

Result<Ok, nsresult>
Trex::Parse(Box& aBox)
{
  BoxReader reader(aBox);

  MOZ_TRY_VAR(mFlags, reader->ReadU32());
  MOZ_TRY_VAR(mTrackId, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleDescriptionIndex, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleDuration, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleSize, reader->ReadU32());
  MOZ_TRY_VAR(mDefaultSampleFlags, reader->ReadU32());

  return Ok();
}

Tfhd::Tfhd(Box& aBox, Trex& aTrex)
  : Trex(aTrex)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Tfhd, "Parse failed");
  }
}

Result<Ok, nsresult>
Tfhd::Parse(Box& aBox)
{
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

Tfdt::Tfdt(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Tfdt, "Parse failed");
  }
}

Result<Ok, nsresult>
Tfdt::Parse(Box& aBox)
{
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

Edts::Edts(Box& aBox)
  : mMediaStart(0)
  , mEmptyOffset(0)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Edts, "Parse failed");
  }
}

Result<Ok, nsresult>
Edts::Parse(Box& aBox)
{
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
      LOG(Edts, "Multiple empty edit, not handled");
    } else if (media_time == -1) {
      mEmptyOffset = segment_duration;
      emptyEntry = true;
    } else if (i > 1 || (i > 0 && !emptyEntry)) {
      LOG(Edts, "More than one edit entry, not handled. A/V sync will be wrong");
      break;
    } else {
      mMediaStart = media_time;
    }
    MOZ_TRY(reader->ReadU32()); // media_rate_integer and media_rate_fraction
  }

  return Ok();
}

Saiz::Saiz(Box& aBox, AtomType aDefaultType)
  : mAuxInfoType(aDefaultType)
  , mAuxInfoTypeParameter(0)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Saiz, "Parse failed");
  }
}

Result<Ok, nsresult>
Saiz::Parse(Box& aBox)
{
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
      LOG(Saiz, "OOM");
    return Err(NS_ERROR_FAILURE);
    }
    memset(mSampleInfoSize.Elements(), defaultSampleInfoSize, mSampleInfoSize.Length());
  } else {
    if (!reader->ReadArray(mSampleInfoSize, count)) {
      LOG(Saiz, "Incomplete Box (OOM or missing count:%u)", count);
    return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Saio::Saio(Box& aBox, AtomType aDefaultType)
  : mAuxInfoType(aDefaultType)
  , mAuxInfoTypeParameter(0)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Saio, "Parse failed");
  }
}

Result<Ok, nsresult>
Saio::Parse(Box& aBox)
{
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
    LOG(Saiz, "OOM");
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

Sbgp::Sbgp(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Sbgp, "Parse failed");
  }
}

Result<Ok, nsresult>
Sbgp::Parse(Box& aBox)
{
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  const uint8_t version = flags >> 24;
  flags = flags & 0xffffff;

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
      LOG(Sbgp, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Sgpd::Sgpd(Box& aBox)
{
  mValid = Parse(aBox).isOk();
  if (!mValid) {
    LOG(Sgpd, "Parse failed");
  }
}

Result<Ok, nsresult>
Sgpd::Parse(Box& aBox)
{
  BoxReader reader(aBox);

  uint32_t flags;
  MOZ_TRY_VAR(flags, reader->ReadU32());
  const uint8_t version = flags >> 24;
  flags = flags & 0xffffff;

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
      LOG(Sgpd, "OOM");
      return Err(NS_ERROR_FAILURE);
    }
  }
  return Ok();
}

Result<Ok, nsresult>
CencSampleEncryptionInfoEntry::Init(BoxReader& aReader)
{
  // Skip a reserved byte.
  MOZ_TRY(aReader->ReadU8());

  uint8_t possiblePatternInfo;
  MOZ_TRY_VAR(possiblePatternInfo, aReader->ReadU8());
  uint8_t flag;
  MOZ_TRY_VAR(flag, aReader->ReadU8());

  MOZ_TRY_VAR(mIVSize, aReader->ReadU8());

  // Read the key id.
  uint8_t key;
  for (uint32_t i = 0; i < kKeyIdSize; ++i) {
    MOZ_TRY_VAR(key, aReader->ReadU8());
    mKeyId.AppendElement(key);
  }

  mIsEncrypted = flag != 0;

  if (mIsEncrypted) {
    if (mIVSize != 8 && mIVSize != 16) {
      return Err(NS_ERROR_FAILURE);
    }
  } else if (mIVSize != 0) {
    return Err(NS_ERROR_FAILURE);
  }

  return Ok();
}

#undef LOG
}
