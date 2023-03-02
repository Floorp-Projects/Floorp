/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_BidiClass_h_
#define intl_components_BidiClass_h_

#include <cstdint>

namespace mozilla::intl {

/**
 *  Read ftp://ftp.unicode.org/Public/UNIDATA/ReadMe-Latest.txt
 *  section BIDIRECTIONAL PROPERTIES
 *  for the detailed definition of the following categories
 *
 *  The values here must match the equivalents in %bidicategorycode in
 *  mozilla/intl/unicharutil/tools/genUnicodePropertyData.pl,
 *  and must also match the values used by ICU's UCharDirection.
 */
enum class BidiClass : uint8_t {
  LeftToRight = 0,
  RightToLeft = 1,
  EuropeanNumber = 2,
  EuropeanNumberSeparator = 3,
  EuropeanNumberTerminator = 4,
  ArabicNumber = 5,
  CommonNumberSeparator = 6,
  BlockSeparator = 7,
  SegmentSeparator = 8,
  WhiteSpaceNeutral = 9,
  OtherNeutral = 10,
  LeftToRightEmbedding = 11,
  LeftToRightOverride = 12,
  RightToLeftArabic = 13,
  RightToLeftEmbedding = 14,
  RightToLeftOverride = 15,
  PopDirectionalFormat = 16,
  DirNonSpacingMark = 17,
  BoundaryNeutral = 18,
  FirstStrongIsolate = 19,
  LeftToRightIsolate = 20,
  RightToLeftIsolate = 21,
  PopDirectionalIsolate = 22,
  BidiClassCount
};

}  // namespace mozilla::intl

#endif
