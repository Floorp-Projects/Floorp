/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OMXCodecDescriptorUtil.h"

namespace android {

// The utility functions in this file concatenate two AVC/H.264 parameter sets,
// sequence parameter set(SPS) and picture parameter set(PPS), into byte stream
// format or construct AVC decoder config descriptor blob from them.
//
// * NAL unit defined in ISO/IEC 14496-10 7.3.1
// * SPS defined ISO/IEC 14496-10 7.3.2.1.1
// * PPS defined in ISO/IEC 14496-10 7.3.2.2
//
// Byte stream format:
// Start code <0x00 0x00 0x00 0x01>  (4 bytes)
// --- (SPS) NAL unit ---
//   ...                             (3 bits)
//   NAL unit type <0x07>            (5 bits)
//   SPS                             (3+ bytes)
//     Profile                         (1 byte)
//     Compatible profiles             (1 byte)
//     Level                           (1 byte)
//   ...
// --- End ---
// Start code <0x00 0x00 0x00 0x01>  (4 bytes)
// --- (PPS) NAL unit ---
//   ...                             (3 bits)
//   NAL unit type <0x08>            (5 bits)
//   PPS                             (1+ bytes)
//   ...
// --- End ---
//
// Descriptor format: (defined in ISO/IEC 14496-15 5.2.4.1.1)
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

// NAL unit start code.
static const uint8_t kNALUnitStartCode[] = { 0x00, 0x00, 0x00, 0x01 };

// NAL unit types.
enum {
  kNALUnitTypeSPS = 0x07,   // Value for sequence parameter set.
  kNALUnitTypePPS = 0x08,   // Value for picture parameter set.
  kNALUnitTypeBad = -1,     // Malformed data.
};

// Sequence parameter set or picture parameter set.
struct AVCParamSet {
  AVCParamSet(const uint8_t* aPtr, const size_t aSize)
    : mPtr(aPtr)
    , mSize(aSize)
  {
    MOZ_ASSERT(mPtr && mSize > 0);
  }

  size_t Size() {
    return mSize + 2; // 2 more bytes for length value.
  }

  // Append 2 bytes length value and NAL unit bitstream to aOutputBuf.
  void AppendTo(nsTArray<uint8_t>* aOutputBuf)
  {
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

// Convert SPS and PPS data into decoder config descriptor blob. The generated
// blob will be appended to aOutputBuf.
static status_t
ConvertParamSetsToDescriptorBlob(sp<ABuffer>& aSPS, sp<ABuffer>& aPPS,
                                 nsTArray<uint8_t>* aOutputBuf)
{
  // Strip start code in the input.
  AVCParamSet sps(aSPS->data() + sizeof(kNALUnitStartCode),
                  aSPS->size() - sizeof(kNALUnitStartCode));
  AVCParamSet pps(aPPS->data() + sizeof(kNALUnitStartCode),
                  aPPS->size() - sizeof(kNALUnitStartCode));
  size_t paramSetsSize = sps.Size() + pps.Size();

  // Profile/level info in SPS.
  uint8_t* info = aSPS->data() + 5;

  uint8_t header[] = {
    0x01,     // Version.
    info[0],  // Profile.
    info[1],  // Compatible profiles.
    info[2],  // Level.
    0xFF,     // 6 bits reserved value <111111> + 2 bits NAL length type <11>
  };

  // Reserve 1 byte for number of SPS & another 1 for number of PPS.
  aOutputBuf->SetCapacity(sizeof(header) + paramSetsSize + 2);
  // Build the blob.
  aOutputBuf->AppendElements(header, sizeof(header)); // 5 bytes Header.
  aOutputBuf->AppendElement(0xE0 | 1); // 3 bits <111> + 5 bits number of SPS.
  sps.AppendTo(aOutputBuf); // SPS NALU data.
  aOutputBuf->AppendElement(1); // 1 byte number of PPS.
  pps.AppendTo(aOutputBuf); // PPS NALU data.

  return OK;
}

static int
NALType(sp<ABuffer>& aBuffer)
{
  if (aBuffer == nullptr) {
    return kNALUnitTypeBad;
  }
  // Start code?
  uint8_t* data = aBuffer->data();
  if (aBuffer->size() <= 4 ||
      memcmp(data, kNALUnitStartCode, sizeof(kNALUnitStartCode))) {
    return kNALUnitTypeBad;
  }

  return data[4] & 0x1F;
}

// Generate AVC/H.264 decoder config blob.
// See MPEG4Writer::Track::makeAVCCodecSpecificData() and
// MPEG4Writer::Track::writeAvccBox() implementation in libstagefright.
status_t
GenerateAVCDescriptorBlob(sp<AMessage>& aConfigData,
                          nsTArray<uint8_t>* aOutputBuf,
                          OMXVideoEncoder::BlobFormat aFormat)
{
  // Search for parameter sets using key "csd-0" and "csd-1".
  char key[6] = "csd-";
  sp<ABuffer> sps;
  sp<ABuffer> pps;
  for (int i = 0; i < 2; i++) {
    snprintf(key + 4, 2, "%d", i);
    sp<ABuffer> paramSet;
    bool found = aConfigData->findBuffer(key, &paramSet);
    int type = NALType(paramSet);
    bool valid = ((type == kNALUnitTypeSPS) || (type == kNALUnitTypePPS));

    MOZ_ASSERT(found && valid);
    if (!found || !valid) {
      return ERROR_MALFORMED;
    }

    switch (type) {
      case kNALUnitTypeSPS:
        sps = paramSet;
        break;
      case kNALUnitTypePPS:
        pps = paramSet;
        break;
      default:
        NS_NOTREACHED("Should not get here!");
    }
  }

  MOZ_ASSERT(sps != nullptr && pps != nullptr);
  if (sps == nullptr || pps == nullptr) {
    return ERROR_MALFORMED;
  }

  status_t result = OK;
  if (aFormat == OMXVideoEncoder::BlobFormat::AVC_NAL) {
    // SPS + PPS.
    aOutputBuf->AppendElements(sps->data(), sps->size());
    aOutputBuf->AppendElements(pps->data(), pps->size());
    return OK;
  } else {
    status_t result = ConvertParamSetsToDescriptorBlob(sps, pps, aOutputBuf);
    MOZ_ASSERT(result == OK);
    return result;
  }
}

} // namespace android
