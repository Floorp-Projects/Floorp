/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_ANNEX_B_H_
#define DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_ANNEX_B_H_

#include "ErrorList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

template <class>
class nsTArray;

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
  static mozilla::Result<mozilla::Ok, nsresult> ConvertAVCCSampleToAnnexB(
      mozilla::MediaRawData* aSample, bool aAddSPS = true);
  // All conversions assume size of NAL length field is 4 bytes.
  // Convert a sample from HVCC format to Annex B.
  static mozilla::Result<mozilla::Ok, nsresult> ConvertHVCCSampleToAnnexB(
      mozilla::MediaRawData* aSample, bool aAddSPS = true);

  // Convert a sample from Annex B to AVCC.
  // an AVCC extradata must not be set.
  static bool ConvertSampleToAVCC(
      mozilla::MediaRawData* aSample,
      const RefPtr<mozilla::MediaByteBuffer>& aAVCCHeader = nullptr);
  // Convert a sample from Annex B to HVCC. An HVCC extradata must not be set.
  static Result<mozilla::Ok, nsresult> ConvertSampleToHVCC(
      mozilla::MediaRawData* aSample);

  // Covert sample to 4 bytes NALU byte stream.
  static mozilla::Result<mozilla::Ok, nsresult> ConvertAVCCTo4BytesAVCC(
      mozilla::MediaRawData* aSample);
  static mozilla::Result<mozilla::Ok, nsresult> ConvertHVCCTo4BytesHVCC(
      mozilla::MediaRawData* aSample);

  // Parse an AVCC extradata and construct the Annex B sample header.
  static already_AddRefed<mozilla::MediaByteBuffer>
  ConvertAVCCExtraDataToAnnexB(const mozilla::MediaByteBuffer* aExtraData);
  // Parse a HVCC extradata and construct the Annex B sample header.
  static already_AddRefed<mozilla::MediaByteBuffer>
  ConvertHVCCExtraDataToAnnexB(const mozilla::MediaByteBuffer* aExtraData);

  // Returns true if format is AVCC and sample has valid extradata.
  static bool IsAVCC(const mozilla::MediaRawData* aSample);
  // Returns true if format is HVCC and sample has valid extradata.
  static bool IsHVCC(const mozilla::MediaRawData* aSample);
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

  // NALU size can vary, this function converts the sample to always 4 bytes
  // NALU.
  static mozilla::Result<mozilla::Ok, nsresult> ConvertNALUTo4BytesNALU(
      mozilla::MediaRawData* aSample, uint8_t aNALUSize);
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_ANNEX_B_H_
