/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OMXCodecDescriptorUtil.h"

namespace android {

// NAL unit start code.
static const uint8_t kNALUnitStartCode[] = { 0x00, 0x00, 0x00, 0x01 };

// This class is used to generate AVC/H.264 decoder config descriptor blob from
// the sequence parameter set(SPS) + picture parameter set(PPS) data.
//
// SPS + PPS format:
// --- SPS NAL unit ---
//   Start code <0x00 0x00 0x00 0x01>  (4 bytes)
//   NAL unit type <0x07>              (5 bits)
//   SPS                               (1+ bytes)
//   ...
// --- PPS NAL unit ---
//   Start code <0x00 0x00 0x00 0x01>  (4 bytes)
//   NAL unit type <0x08>              (5 bits)
//   PPS                               (1+ bytes)
//   ...
// --- End ---
//
// Descriptor format:
// --- Header (5 bytes) ---
//   Version <0x01>       (1 byte)
//   Profile              (1 byte)
//   Compatible profiles  (1 byte)
//   Level                (1 byte)
//   Reserved <111111>    (6 bits)
//   NAL length type      (2 bits)
// --- Parameter sets ---
//   Reserved <111>       (3 bits)
//   Number of SPS        (5 bits)
//   SPS                  (3+ bytes)
//     Length               (2 bytes)
//     SPS NAL unit         (1+ bytes)
//   ...
//   Number of PPS        (1 byte)
//   PPS                  (3+ bytes)
//     Length               (2 bytes)
//     PPS NAL unit         (1+ bytes)
//   ...
// --- End ---
class AVCDecodeConfigDescMaker {
public:
  // Convert SPS + PPS data to decoder config descriptor blob. aParamSets
  // contains the source data, and the generated blob will be appended to
  // aOutputBuf.
  status_t ConvertParamSetsToDescriptorBlob(ABuffer* aParamSets,
                                            nsTArray<uint8_t>* aOutputBuf)
  {
    uint8_t header[] = {
      0x01, // Version.
      0x00, // Will be filled with 'profile' when parsing SPS later.
      0x00, // Will be filled with 'compatible profiles' when parsing SPS later.
      0x00, // Will be filled with 'level' when parsing SPS later.
      0xFF, // 6 bits reserved value <111111> + 2 bits NAL length type <11>
    };

    size_t paramSetsSize = ParseParamSets(aParamSets, header);
    NS_ENSURE_TRUE(paramSetsSize > 0, ERROR_MALFORMED);

    // Extra 1 byte for number of SPS & the other for number of PPS.
    aOutputBuf->SetCapacity(sizeof(header) + paramSetsSize + 2);
    // 5 bytes Header.
    aOutputBuf->AppendElements(header, sizeof(header));
    // 3 bits <111> + 5 bits number of SPS.
    uint8_t n = mSPS.Length();
    aOutputBuf->AppendElement(0xE0 | n);
    // SPS NAL units.
    for (int i = 0; i < n; i++) {
      mSPS.ElementAt(i).AppendTo(aOutputBuf);
    }
    // 1 byte number of PPS.
    n = mPPS.Length();
    aOutputBuf->AppendElement(n);
    // PPS NAL units.
    for (int i = 0; i < n; i++) {
      mPPS.ElementAt(i).AppendTo(aOutputBuf);
    }

    return OK;
  }

private:
  // Sequence parameter set or picture parameter set.
  struct AVCParamSet {
    AVCParamSet(const uint8_t* aPtr, const size_t aSize)
      : mPtr(aPtr)
      , mSize(aSize)
    {}

    // Append 2 bytes length value and NAL unit bitstream to aOutputBuf.
    void AppendTo(nsTArray<uint8_t>* aOutputBuf)
    {
      MOZ_ASSERT(mPtr && mSize > 0);

      // 2 bytes length value.
      uint8_t size[] = {
        (mSize & 0xFF00) >> 8, // MSB.
        mSize & 0x00FF,        // LSB.
      };
      aOutputBuf->AppendElements(size, sizeof(size));

      aOutputBuf->AppendElements(mPtr, mSize);
    }

    const uint8_t* mPtr; // Pointer to NAL unit.
    const size_t mSize;  // NAL unit length in bytes.
  };

  // NAL unit types.
  enum {
    kNALUnitTypeSPS = 0x07, // Value for sequence parameter set.
    kNALUnitTypePPS = 0x08, // Value for picture parameter set.
  };

  // Search for next start code to determine the location of parameter set data
  // and save the result to corresponding parameter set arrays. The search range
  // is from aPtr to (aPtr + aSize - 4), and aType indicates which array to save
  // the result.
  // The size (in bytes) of found parameter set will be stored in
  // aParameterSize.
  // This function also returns the pointer to found start code that caller can
  // use for the next iteration of search. If the returned pointer is beyond
  // the end of search range, it means no start code is found.
  uint8_t* ParseParamSet(uint8_t* aPtr, size_t aSize, uint8_t aType,
                         size_t* aParamSetSize)
  {
    MOZ_ASSERT(aPtr && aSize > 0);
    MOZ_ASSERT(aType == kNALUnitTypeSPS || aType == kNALUnitTypePPS);
    MOZ_ASSERT(aParamSetSize);

    // Find next start code.
    size_t index = 0;
    size_t end = aSize - sizeof(kNALUnitStartCode);
    uint8_t* nextStartCode = aPtr;
    while (index <= end &&
            memcmp(kNALUnitStartCode, aPtr + index, sizeof(kNALUnitStartCode))) {
      ++index;
    }
    if (index <= end) {
      // Found.
      nextStartCode = aPtr + index;
    } else {
      nextStartCode = aPtr + aSize;
    }

    *aParamSetSize = nextStartCode - aPtr;
    NS_ENSURE_TRUE(*aParamSetSize > 0, nullptr);

    AVCParamSet paramSet(aPtr, *aParamSetSize);
    if (aType == kNALUnitTypeSPS) {
      // SPS should have at least 4 bytes.
      NS_ENSURE_TRUE(*aParamSetSize >= 4, nullptr);
      mSPS.AppendElement(paramSet);
    } else {
      mPPS.AppendElement(paramSet);
    }
    return nextStartCode;
  }

  // Walk through SPS + PPS data and save the pointer & size of each parameter
  // set to corresponding arrays. It also fills several values in aHeader.
  // Will return total size of all parameter sets, or 0 if fail to parse.
  size_t ParseParamSets(ABuffer* aParamSets, uint8_t* aHeader)
  {
    // Data starts with a start code.
    // SPS and PPS are separated with start codes.
    // Also, SPS must come before PPS
    uint8_t type = kNALUnitTypeSPS;
    bool hasSPS = false;
    bool hasPPS = false;
    uint8_t* ptr = aParamSets->data();
    uint8_t* nextStartCode = ptr;
    size_t remain = aParamSets->size();
    size_t paramSetSize = 0;
    size_t totalSize = 0;
    // Examine
    while (remain > sizeof(kNALUnitStartCode) &&
            !memcmp(kNALUnitStartCode, ptr, sizeof(kNALUnitStartCode))) {
      ptr += sizeof(kNALUnitStartCode);
      remain -= sizeof(kNALUnitStartCode);
      // NAL unit format is defined in ISO/IEC 14496-10 7.3.1:
      // --- NAL unit ---
      // Reserved <111>         (3 bits)
      // Type                   (5 bits)
      // Parameter set          (4+ bytes for SPS, 1+ bytes for PPS)
      // --- End ---
      type = (ptr[0] & 0x1F);
      if (type == kNALUnitTypeSPS) {
        // SPS must come before PPS.
        NS_ENSURE_FALSE(hasPPS, 0);
        if (!hasSPS) {
          // SPS contains some header values.
          aHeader[1] = ptr[1]; // Profile.
          aHeader[2] = ptr[2]; // Compatible Profiles.
          aHeader[3] = ptr[3]; // Level.

          hasSPS = true;
        }
        nextStartCode = ParseParamSet(ptr, remain, type, &paramSetSize);
      } else if (type == kNALUnitTypePPS) {
        // SPS must come before PPS.
        NS_ENSURE_TRUE(hasSPS, 0);
        if (!hasPPS) {
          hasPPS = true;
        }
        nextStartCode = ParseParamSet(ptr, remain, type, &paramSetSize);
      } else {
        // Should never contain NAL unit other than SPS or PPS.
        NS_ENSURE_TRUE(false, 0);
      }
      NS_ENSURE_TRUE(nextStartCode, 0);

      // Move to next start code.
      remain -= (nextStartCode - ptr);
      ptr = nextStartCode;
      totalSize += (2 + paramSetSize); // 2 bytes length + NAL unit.
    }

    // Sanity check on the number of parameter sets.
    size_t n = mSPS.Length();
    NS_ENSURE_TRUE(n > 0 && n <= 0x1F, 0); // 5 bits length only.
    n = mPPS.Length();
    NS_ENSURE_TRUE(n > 0 && n <= 0xFF, 0); // 1 byte length only.

    return totalSize;
  }

  nsTArray<AVCParamSet> mSPS;
  nsTArray<AVCParamSet> mPPS;
};

// Blob from OMX encoder could be in descriptor format already, or sequence
// parameter set(SPS) + picture parameter set(PPS). If later, it needs to be
// parsed and converted into descriptor format.
// See MPEG4Writer::Track::makeAVCCodecSpecificData() and
// MPEG4Writer::Track::writeAvccBox() implementation in libstagefright.
status_t
GenerateAVCDescriptorBlob(ABuffer* aData, nsTArray<uint8_t>* aOutputBuf)
{
  const size_t csdSize = aData->size();
  const uint8_t* csd = aData->data();

  MOZ_ASSERT(csdSize > sizeof(kNALUnitStartCode),
             "Size of codec specific data is too short. "
             "There could be a serious problem in MediaCodec.");

  NS_ENSURE_TRUE(csdSize > sizeof(kNALUnitStartCode), ERROR_MALFORMED);

  if (memcmp(csd, kNALUnitStartCode, sizeof(kNALUnitStartCode))) {
    // Already in descriptor format. It should has at least 13 bytes.
    NS_ENSURE_TRUE(csdSize >= 13, ERROR_MALFORMED);

    aOutputBuf->AppendElements(aData->data(), csdSize);
  } else {
    // In SPS + PPS format. Generate descriptor blob from parameters sets.
    AVCDecodeConfigDescMaker maker;
    status_t result = maker.ConvertParamSetsToDescriptorBlob(aData, aOutputBuf);
    NS_ENSURE_TRUE(result == OK, result);
  }

  return OK;
}

} // namespace android