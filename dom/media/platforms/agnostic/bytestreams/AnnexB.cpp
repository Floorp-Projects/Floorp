/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "AnnexB.h"
#include "BufferReader.h"
#include "ByteWriter.h"
#include "MediaData.h"

namespace mozilla {

static const uint8_t kAnnexBDelimiter[] = {0, 0, 0, 1};

Result<Ok, nsresult> AnnexB::ConvertSampleToAnnexB(
    mozilla::MediaRawData* aSample, bool aAddSPS) {
  MOZ_ASSERT(aSample);

  if (!IsAVCC(aSample)) {
    return Ok();
  }
  MOZ_ASSERT(aSample->Data());

  MOZ_TRY(ConvertSampleTo4BytesAVCC(aSample));

  if (aSample->Size() < 4) {
    // Nothing to do, it's corrupted anyway.
    return Ok();
  }

  BufferReader reader(aSample->Data(), aSample->Size());

  nsTArray<uint8_t> tmp;
  ByteWriter<BigEndian> writer(tmp);

  while (reader.Remaining() >= 4) {
    uint32_t nalLen;
    MOZ_TRY_VAR(nalLen, reader.ReadU32());
    const uint8_t* p = reader.Read(nalLen);

    if (!writer.Write(kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter))) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
    if (!p) {
      break;
    }
    if (!writer.Write(p, nalLen)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
  }

  UniquePtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());

  if (!samplewriter->Replace(tmp.Elements(), tmp.Length())) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  // Prepend the Annex B NAL with SPS and PPS tables to keyframes.
  if (aAddSPS && aSample->mKeyframe) {
    RefPtr<MediaByteBuffer> annexB =
        ConvertExtraDataToAnnexB(aSample->mExtraData);
    if (!samplewriter->Prepend(annexB->Elements(), annexB->Length())) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }

    // Prepending the NAL with SPS/PPS will mess up the encryption subsample
    // offsets. So we need to account for the extra bytes by increasing
    // the length of the first clear data subsample. Otherwise decryption
    // will fail.
    if (aSample->mCrypto.IsEncrypted()) {
      if (aSample->mCrypto.mPlainSizes.Length() == 0) {
        samplewriter->mCrypto.mPlainSizes.AppendElement(annexB->Length());
        samplewriter->mCrypto.mEncryptedSizes.AppendElement(
            samplewriter->Size() - annexB->Length());
      } else {
        samplewriter->mCrypto.mPlainSizes[0] += annexB->Length();
      }
    }
  }

  return Ok();
}

already_AddRefed<mozilla::MediaByteBuffer> AnnexB::ConvertExtraDataToAnnexB(
    const mozilla::MediaByteBuffer* aExtraData) {
  // AVCC 6 byte header looks like:
  //     +------+------+------+------+------+------+------+------+
  // [0] |   0  |   0  |   0  |   0  |   0  |   0  |   0  |   1  |
  //     +------+------+------+------+------+------+------+------+
  // [1] | profile                                               |
  //     +------+------+------+------+------+------+------+------+
  // [2] | compatiblity                                          |
  //     +------+------+------+------+------+------+------+------+
  // [3] | level                                                 |
  //     +------+------+------+------+------+------+------+------+
  // [4] | unused                                  | nalLenSiz-1 |
  //     +------+------+------+------+------+------+------+------+
  // [5] | unused             | numSps                           |
  //     +------+------+------+------+------+------+------+------+

  RefPtr<mozilla::MediaByteBuffer> annexB = new mozilla::MediaByteBuffer;

  BufferReader reader(*aExtraData);
  const uint8_t* ptr = reader.Read(5);
  if (ptr && ptr[0] == 1) {
    // Append SPS then PPS
    Unused << reader.ReadU8().map(
        [&](uint8_t x) { return ConvertSPSOrPPS(reader, x & 31, annexB); });
    Unused << reader.ReadU8().map(
        [&](uint8_t x) { return ConvertSPSOrPPS(reader, x, annexB); });
    // MP4Box adds extra bytes that we ignore. I don't know what they do.
  }

  return annexB.forget();
}

Result<mozilla::Ok, nsresult> AnnexB::ConvertSPSOrPPS(
    BufferReader& aReader, uint8_t aCount, mozilla::MediaByteBuffer* aAnnexB) {
  for (int i = 0; i < aCount; i++) {
    uint16_t length;
    MOZ_TRY_VAR(length, aReader.ReadU16());

    const uint8_t* ptr = aReader.Read(length);
    if (!ptr) {
      return Err(NS_ERROR_FAILURE);
    }
    aAnnexB->AppendElements(kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter));
    aAnnexB->AppendElements(ptr, length);
  }
  return Ok();
}

static Result<Ok, nsresult> FindStartCodeInternal(BufferReader& aBr) {
  size_t offset = aBr.Offset();

  for (uint32_t i = 0; i < aBr.Align() && aBr.Remaining() >= 3; i++) {
    auto res = aBr.PeekU24();
    if (res.isOk() && (res.unwrap() == 0x000001)) {
      return Ok();
    }
    mozilla::Unused << aBr.Read(1);
  }

  while (aBr.Remaining() >= 6) {
    uint32_t x32;
    MOZ_TRY_VAR(x32, aBr.PeekU32());
    if ((x32 - 0x01010101) & (~x32) & 0x80808080) {
      if ((x32 >> 8) == 0x000001) {
        return Ok();
      }
      if (x32 == 0x000001) {
        mozilla::Unused << aBr.Read(1);
        return Ok();
      }
      if ((x32 & 0xff) == 0) {
        const uint8_t* p = aBr.Peek(1);
        if ((x32 & 0xff00) == 0 && p[4] == 1) {
          mozilla::Unused << aBr.Read(2);
          return Ok();
        }
        if (p[4] == 0 && p[5] == 1) {
          mozilla::Unused << aBr.Read(3);
          return Ok();
        }
      }
    }
    mozilla::Unused << aBr.Read(4);
  }

  while (aBr.Remaining() >= 3) {
    uint32_t data;
    MOZ_TRY_VAR(data, aBr.PeekU24());
    if (data == 0x000001) {
      return Ok();
    }
    mozilla::Unused << aBr.Read(1);
  }

  // No start code were found; Go back to the beginning.
  mozilla::Unused << aBr.Seek(offset);
  return Err(NS_ERROR_FAILURE);
}

static Result<Ok, nsresult> FindStartCode(BufferReader& aBr,
                                          size_t& aStartSize) {
  if (FindStartCodeInternal(aBr).isErr()) {
    aStartSize = 0;
    return Err(NS_ERROR_FAILURE);
  }

  aStartSize = 3;
  if (aBr.Offset()) {
    // Check if it's 4-bytes start code
    aBr.Rewind(1);
    uint8_t data;
    MOZ_TRY_VAR(data, aBr.ReadU8());
    if (data == 0) {
      aStartSize = 4;
    }
  }
  mozilla::Unused << aBr.Read(3);
  return Ok();
}

/* static */
void AnnexB::ParseNALEntries(const Span<const uint8_t>& aSpan,
                             nsTArray<AnnexB::NALEntry>& aEntries) {
  BufferReader reader(aSpan.data(), aSpan.Length());
  size_t startSize;
  auto rv = FindStartCode(reader, startSize);
  size_t startOffset = reader.Offset();
  if (rv.isOk()) {
    while (FindStartCode(reader, startSize).isOk()) {
      int64_t offset = reader.Offset();
      int64_t sizeNAL = offset - startOffset - startSize;
      aEntries.AppendElement(AnnexB::NALEntry(startOffset, sizeNAL));
      reader.Seek(startOffset);
      reader.Read(sizeNAL + startSize);
      startOffset = offset;
    }
  }
  int64_t sizeNAL = reader.Remaining();
  if (sizeNAL) {
    aEntries.AppendElement(AnnexB::NALEntry(startOffset, sizeNAL));
  }
}

static Result<mozilla::Ok, nsresult> ParseNALUnits(ByteWriter<BigEndian>& aBw,
                                                   BufferReader& aBr) {
  size_t startSize;

  auto rv = FindStartCode(aBr, startSize);
  if (rv.isOk()) {
    size_t startOffset = aBr.Offset();
    while (FindStartCode(aBr, startSize).isOk()) {
      size_t offset = aBr.Offset();
      size_t sizeNAL = offset - startOffset - startSize;
      aBr.Seek(startOffset);
      if (!aBw.WriteU32(sizeNAL) || !aBw.Write(aBr.Read(sizeNAL), sizeNAL)) {
        return Err(NS_ERROR_OUT_OF_MEMORY);
      }
      aBr.Read(startSize);
      startOffset = offset;
    }
  }
  size_t sizeNAL = aBr.Remaining();
  if (sizeNAL) {
    if (!aBw.WriteU32(sizeNAL) || !aBw.Write(aBr.Read(sizeNAL), sizeNAL)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
  }
  return Ok();
}

bool AnnexB::ConvertSampleToAVCC(mozilla::MediaRawData* aSample,
                                 const RefPtr<MediaByteBuffer>& aAVCCHeader) {
  if (IsAVCC(aSample)) {
    return ConvertSampleTo4BytesAVCC(aSample).isOk();
  }
  if (!IsAnnexB(aSample)) {
    // Not AnnexB, nothing to convert.
    return true;
  }

  nsTArray<uint8_t> nalu;
  ByteWriter<BigEndian> writer(nalu);
  BufferReader reader(aSample->Data(), aSample->Size());

  if (ParseNALUnits(writer, reader).isErr()) {
    return false;
  }
  UniquePtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());
  if (!samplewriter->Replace(nalu.Elements(), nalu.Length())) {
    return false;
  }

  if (aAVCCHeader) {
    aSample->mExtraData = aAVCCHeader;
    return true;
  }

  // Create the AVCC header.
  auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
  static const uint8_t kFakeExtraData[] = {
      1 /* version */,
      0x64 /* profile (High) */,
      0 /* profile compat (0) */,
      40 /* level (40) */,
      0xfc | 3 /* nal size - 1 */,
      0xe0 /* num SPS (0) */,
      0 /* num PPS (0) */
  };
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  extradata->AppendElements(kFakeExtraData, ArrayLength(kFakeExtraData));
  aSample->mExtraData = std::move(extradata);
  return true;
}

Result<mozilla::Ok, nsresult> AnnexB::ConvertSampleTo4BytesAVCC(
    mozilla::MediaRawData* aSample) {
  MOZ_ASSERT(IsAVCC(aSample));

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;

  if (nalLenSize == 4) {
    return Ok();
  }
  nsTArray<uint8_t> dest;
  ByteWriter<BigEndian> writer(dest);
  BufferReader reader(aSample->Data(), aSample->Size());
  while (reader.Remaining() > nalLenSize) {
    uint32_t nalLen;
    switch (nalLenSize) {
      case 1:
        MOZ_TRY_VAR(nalLen, reader.ReadU8());
        break;
      case 2:
        MOZ_TRY_VAR(nalLen, reader.ReadU16());
        break;
      case 3:
        MOZ_TRY_VAR(nalLen, reader.ReadU24());
        break;
    }

    MOZ_ASSERT(nalLenSize != 4);

    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return Ok();
    }
    if (!writer.WriteU32(nalLen) || !writer.Write(p, nalLen)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
  }
  UniquePtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());
  if (!samplewriter->Replace(dest.Elements(), dest.Length())) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }
  return Ok();
}

bool AnnexB::IsAVCC(const mozilla::MediaRawData* aSample) {
  return aSample->Size() >= 3 && aSample->mExtraData &&
         aSample->mExtraData->Length() >= 7 && (*aSample->mExtraData)[0] == 1;
}

bool AnnexB::IsAnnexB(const mozilla::MediaRawData* aSample) {
  if (aSample->Size() < 4) {
    return false;
  }
  uint32_t header = mozilla::BigEndian::readUint32(aSample->Data());
  return header == 0x00000001 || (header >> 8) == 0x000001;
}

}  // namespace mozilla
