/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_ANNEX_B_H_
#define MP4_DEMUXER_ANNEX_B_H_

#include "mozilla/Result.h"
#include "ErrorList.h"

template <class T>
struct already_AddRefed;

namespace mozilla {
class BufferReader;
class MediaRawData;
class MediaByteBuffer;

class AnnexB {
 public:
  struct NALEntry {
    NALEntry(int64_t aOffset, int64_t aSize) : mOffset(aOffset), mSize(aSize) {
      MOZ_ASSERT(mOffset >= 0);
      MOZ_ASSERT(mSize >= 0);
    }
    // They should be non-negative, so we use int64_t to assert their value when
    // assigning value to them.
    int64_t mOffset;
    int64_t mSize;
  };
  // All conversions assume size of NAL length field is 4 bytes.
  // Convert a sample from AVCC format to Annex B.
  static mozilla::Result<mozilla::Ok, nsresult> ConvertSampleToAnnexB(
      mozilla::MediaRawData* aSample, bool aAddSPS = true);
  // Convert a sample from Annex B to AVCC.
  // an AVCC extradata must not be set.
  static bool ConvertSampleToAVCC(
      mozilla::MediaRawData* aSample,
      const RefPtr<mozilla::MediaByteBuffer>& aAVCCHeader = nullptr);
  static mozilla::Result<mozilla::Ok, nsresult> ConvertSampleTo4BytesAVCC(
      mozilla::MediaRawData* aSample);

  // Parse an AVCC extradata and construct the Annex B sample header.
  static already_AddRefed<mozilla::MediaByteBuffer> ConvertExtraDataToAnnexB(
      const mozilla::MediaByteBuffer* aExtraData);
  // Returns true if format is AVCC and sample has valid extradata.
  static bool IsAVCC(const mozilla::MediaRawData* aSample);
  // Returns true if format is AnnexB.
  static bool IsAnnexB(const mozilla::MediaRawData* aSample);

  // Parse NAL entries from the bytes stream to know the offset and the size of
  // each NAL in the bytes stream.
  static void ParseNALEntries(const Span<const uint8_t>& aSpan,
                              nsTArray<AnnexB::NALEntry>& aEntries);

 private:
  // AVCC box parser helper.
  static mozilla::Result<mozilla::Ok, nsresult> ConvertSPSOrPPS(
      mozilla::BufferReader& aReader, uint8_t aCount,
      mozilla::MediaByteBuffer* aAnnexB);
};

}  // namespace mozilla

#endif  // MP4_DEMUXER_ANNEX_B_H_
