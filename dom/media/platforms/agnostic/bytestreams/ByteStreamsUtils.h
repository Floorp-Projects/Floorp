/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_BYTESTREAMUTILS_H_
#define DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_BYTESTREAMUTILS_H_

namespace mozilla {

// Described in ISO 23001-8:2016
// H264 spec Table 2, H265 spec Table E.3
enum class PrimaryID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  BT470M = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  FILM = 8,
  BT2020 = 9,
  SMPTEST428_1 = 10,
  SMPTEST431_2 = 11,
  SMPTEST432_1 = 12,
  EBU_3213_E = 22
};

// H264 spec Table 3,  H265 spec Table E.4
enum class TransferID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  GAMMA22 = 4,
  GAMMA28 = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  LINEAR = 8,
  LOG = 9,
  LOG_SQRT = 10,
  IEC61966_2_4 = 11,
  BT1361_ECG = 12,
  IEC61966_2_1 = 13,
  BT2020_10 = 14,
  BT2020_12 = 15,
  SMPTEST2084 = 16,
  SMPTEST428_1 = 17,

  // Not yet standardized
  ARIB_STD_B67 = 18,  // AKA hybrid-log gamma, HLG.
};

// H264 spec Table 4,  H265 spec Table E.5
enum class MatrixID : uint8_t {
  RGB = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  FCC = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  YCOCG = 8,
  BT2020_NCL = 9,
  BT2020_CL = 10,
  YDZDX = 11,
  INVALID = 255,
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
