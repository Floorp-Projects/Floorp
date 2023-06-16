/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MP4_SAMPLE_ITERATOR_H_
#define DOM_MEDIA_MP4_SAMPLE_ITERATOR_H_

#include "ByteStream.h"
#include "MediaData.h"
#include "MediaResource.h"
#include "MoofParser.h"
#include "mozilla/ResultVariant.h"
#include "MP4Interval.h"
#include "nsISupportsImpl.h"
#include "TimeUnits.h"

namespace mozilla {

struct CencSampleEncryptionInfoEntry;
class IndiceWrapper;
class MP4SampleIndex;
struct Sample;

class SampleIterator {
 public:
  explicit SampleIterator(MP4SampleIndex* aIndex);
  ~SampleIterator();
  bool HasNext();
  already_AddRefed<mozilla::MediaRawData> GetNext();
  void Seek(const media::TimeUnit& aTime);
  media::TimeUnit GetNextKeyframeTime();

 private:
  Sample* Get();

  // Gets the sample description entry for the current moof, or nullptr if
  // called without a valid current moof.
  SampleDescriptionEntry* GetSampleDescriptionEntry();
  CencSampleEncryptionInfoEntry* GetSampleEncryptionEntry();

  // Determines the encryption scheme in use for the current sample. If the
  // the scheme cannot be unambiguously determined, will return an error with
  // the reason.
  //
  // Returns: Ok(CryptoScheme) if a crypto scheme, including None, can be
  // determined, or Err(nsCString) if there is an issue determining the scheme.
  Result<CryptoScheme, nsCString> GetEncryptionScheme();

  void Next();
  RefPtr<MP4SampleIndex> mIndex;
  friend class MP4SampleIndex;
  size_t mCurrentMoof;
  size_t mCurrentSample;
};

class MP4SampleIndex {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MP4SampleIndex)

  struct Indice {
    uint64_t start_offset;
    uint64_t end_offset;
    int64_t start_composition;
    int64_t end_composition;
    int64_t start_decode;
    bool sync;
  };

  struct MP4DataOffset {
    MP4DataOffset(uint32_t aIndex, int64_t aStartOffset)
        : mIndex(aIndex), mStartOffset(aStartOffset), mEndOffset(0) {}

    bool operator==(int64_t aStartOffset) const {
      return mStartOffset == aStartOffset;
    }

    bool operator!=(int64_t aStartOffset) const {
      return mStartOffset != aStartOffset;
    }

    bool operator<(int64_t aStartOffset) const {
      return mStartOffset < aStartOffset;
    }

    struct EndOffsetComparator {
      bool Equals(const MP4DataOffset& a, const int64_t& b) const {
        return a.mEndOffset == b;
      }

      bool LessThan(const MP4DataOffset& a, const int64_t& b) const {
        return a.mEndOffset < b;
      }
    };

    uint32_t mIndex;
    int64_t mStartOffset;
    int64_t mEndOffset;
    MP4Interval<media::TimeUnit> mTime;
  };

  MP4SampleIndex(const mozilla::IndiceWrapper& aIndices, ByteStream* aSource,
                 uint32_t aTrackId, bool aIsAudio, uint32_t aTimeScale);

  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges,
                       bool aCanEvict);
  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges);
  media::TimeUnit GetEndCompositionIfBuffered(
      const mozilla::MediaByteRangeSet& aByteRanges);
  mozilla::media::TimeIntervals ConvertByteRangesToTimeRanges(
      const mozilla::MediaByteRangeSet& aByteRanges);
  uint64_t GetEvictionOffset(const media::TimeUnit& aTime);
  bool IsFragmented() { return !!mMoofParser; }

  friend class SampleIterator;

 private:
  ~MP4SampleIndex();
  void RegisterIterator(SampleIterator* aIterator);
  void UnregisterIterator(SampleIterator* aIterator);

  ByteStream* mSource;
  FallibleTArray<Sample> mIndex;
  FallibleTArray<MP4DataOffset> mDataOffset;
  UniquePtr<MoofParser> mMoofParser;
  nsTArray<SampleIterator*> mIterators;

  // ConvertByteRangesToTimeRanges cache
  mozilla::MediaByteRangeSet mLastCachedRanges;
  mozilla::media::TimeIntervals mLastBufferedRanges;
  bool mIsAudio;
};
}  // namespace mozilla

#endif
