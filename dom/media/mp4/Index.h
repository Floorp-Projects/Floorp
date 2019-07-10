/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INDEX_H_
#define INDEX_H_

#include "ByteStream.h"
#include "MediaData.h"
#include "MediaResource.h"
#include "MoofParser.h"
#include "mozilla/Result.h"
#include "MP4Interval.h"
#include "nsISupportsImpl.h"
#include "TimeUnits.h"

template <class T>
class nsAutoPtr;

namespace mozilla {
class IndiceWrapper;
struct Sample;
struct CencSampleEncryptionInfoEntry;

class Index;

typedef int64_t Microseconds;

class SampleIterator {
 public:
  explicit SampleIterator(Index* aIndex);
  ~SampleIterator();
  already_AddRefed<mozilla::MediaRawData> GetNext();
  void Seek(Microseconds aTime);
  Microseconds GetNextKeyframeTime();

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
  Result<CryptoScheme, const nsCString> GetEncryptionScheme();

  void Next();
  RefPtr<Index> mIndex;
  friend class Index;
  size_t mCurrentMoof;
  size_t mCurrentSample;
};

class Index {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Index)

  struct Indice {
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t start_composition;
    uint64_t end_composition;
    uint64_t start_decode;
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
    MP4Interval<Microseconds> mTime;
  };

  Index(const mozilla::IndiceWrapper& aIndices, ByteStream* aSource,
        uint32_t aTrackId, bool aIsAudio);

  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges,
                       bool aCanEvict);
  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges);
  Microseconds GetEndCompositionIfBuffered(
      const mozilla::MediaByteRangeSet& aByteRanges);
  mozilla::media::TimeIntervals ConvertByteRangesToTimeRanges(
      const mozilla::MediaByteRangeSet& aByteRanges);
  uint64_t GetEvictionOffset(Microseconds aTime);
  bool IsFragmented() { return mMoofParser; }

  friend class SampleIterator;

 private:
  ~Index();
  void RegisterIterator(SampleIterator* aIterator);
  void UnregisterIterator(SampleIterator* aIterator);

  ByteStream* mSource;
  FallibleTArray<Sample> mIndex;
  FallibleTArray<MP4DataOffset> mDataOffset;
  nsAutoPtr<MoofParser> mMoofParser;
  nsTArray<SampleIterator*> mIterators;

  // ConvertByteRangesToTimeRanges cache
  mozilla::MediaByteRangeSet mLastCachedRanges;
  mozilla::media::TimeIntervals mLastBufferedRanges;
  bool mIsAudio;
};
}  // namespace mozilla

#endif
