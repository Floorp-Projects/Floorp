/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EXIF.h"

namespace mozilla {
namespace image {

// Section references in this file refer to the EXIF v2.3 standard, also known
// as CIPA DC-008-Translation-2010.

// See Section 4.6.4, Table 4.
// Typesafe enums are intentionally not used here since we're comparing to raw
// integers produced by parsing.
enum EXIFTag
{
  OrientationTag = 0x112,
};

// See Section 4.6.2.
enum EXIFType
{
  ByteType       = 1,
  ASCIIType      = 2,
  ShortType      = 3,
  LongType       = 4,
  RationalType   = 5,
  UndefinedType  = 7,
  SignedLongType = 9,
  SignedRational = 10,
};

static const char* EXIFHeader = "Exif\0\0";
static const uint32_t EXIFHeaderLength = 6;

/////////////////////////////////////////////////////////////
// Parse EXIF data, typically found in a JPEG's APP1 segment.
/////////////////////////////////////////////////////////////
EXIFData
EXIFParser::ParseEXIF(const uint8_t* aData, const uint32_t aLength)
{
  if (!Initialize(aData, aLength))
    return EXIFData();

  if (!ParseEXIFHeader())
    return EXIFData();

  uint32_t offsetIFD;
  if (!ParseTIFFHeader(offsetIFD))
    return EXIFData();

  JumpTo(offsetIFD);

  Orientation orientation;
  if (!ParseIFD0(orientation))
    return EXIFData();

  // We only care about orientation at this point, so we don't bother with the
  // other IFDs. If we got this far we're done.
  return EXIFData(orientation);
}

/////////////////////////////////////////////////////////
// Parse the EXIF header. (Section 4.7.2, Figure 30)
/////////////////////////////////////////////////////////
bool
EXIFParser::ParseEXIFHeader()
{
  return MatchString(EXIFHeader, EXIFHeaderLength);
}

/////////////////////////////////////////////////////////
// Parse the TIFF header. (Section 4.5.2, Table 1)
/////////////////////////////////////////////////////////
bool
EXIFParser::ParseTIFFHeader(uint32_t& aIFD0OffsetOut)
{
  // Determine byte order.
  if (MatchString("MM", 2))
    mByteOrder = ByteOrder::BigEndian;
  else if (MatchString("II", 2))
    mByteOrder = ByteOrder::LittleEndian;
  else
    return false;

  if (!MatchString("\0*", 2))
    return false;

  // Determine offset of the 0th IFD. (It shouldn't be greater than 64k, which
  // is the maximum size of the entry APP1 segment.)
  uint32_t ifd0Offset;
  if (!ReadUInt32(ifd0Offset) || ifd0Offset > 64 * 1024)
    return false;

  // The IFD offset is relative to the beginning of the TIFF header, which
  // begins after the EXIF header, so we need to increase the offset
  // appropriately.
  aIFD0OffsetOut = ifd0Offset + EXIFHeaderLength;
  return true;
}

/////////////////////////////////////////////////////////
// Parse the entries in IFD0. (Section 4.6.2)
/////////////////////////////////////////////////////////
bool
EXIFParser::ParseIFD0(Orientation& aOrientationOut)
{
  uint16_t entryCount;
  if (!ReadUInt16(entryCount))
    return false;

  for (uint16_t entry = 0 ; entry < entryCount ; ++entry) {
    // Read the fields of the entry.
    uint16_t tag;
    if (!ReadUInt16(tag))
      return false;

    // Right now, we only care about orientation, so we immediately skip to the
    // next entry if we find anything else.
    if (tag != OrientationTag) {
      Advance(10);
      continue;
    }

    uint16_t type;
    if (!ReadUInt16(type))
      return false;

    uint32_t count;
    if (!ReadUInt32(count))
      return false;

    // We should have an orientation value here; go ahead and parse it.
    Orientation orientation;
    if (!ParseOrientation(type, count, aOrientationOut))
      return false;

    // Since the orientation is all we care about, we're done.
    return true;
  }

  // We didn't find an orientation field in the IFD. That's OK; we assume the
  // default orientation in that case.
  aOrientationOut = Orientation();
  return true;
}

bool
EXIFParser::ParseOrientation(uint16_t aType, uint32_t aCount, Orientation& aOut)
{
  // Sanity check the type and count.
  if (aType != ShortType || aCount != 1)
    return false;

  uint16_t value;
  if (!ReadUInt16(value))
    return false;

  switch (value) {
    case 1: aOut = Orientation(Angle::D0,   Flip::Unflipped);  break;
    case 2: aOut = Orientation(Angle::D0,   Flip::Horizontal); break;
    case 3: aOut = Orientation(Angle::D180, Flip::Unflipped);  break;
    case 4: aOut = Orientation(Angle::D180, Flip::Horizontal); break;
    case 5: aOut = Orientation(Angle::D90,  Flip::Horizontal); break;
    case 6: aOut = Orientation(Angle::D90,  Flip::Unflipped);  break;
    case 7: aOut = Orientation(Angle::D270, Flip::Horizontal); break;
    case 8: aOut = Orientation(Angle::D270, Flip::Unflipped);  break;
    default: return false;
  }

  // This is a 32-bit field, but the orientation value only occupies the first
  // 16 bits. We need to advance another 16 bits to consume the entire field.
  Advance(2);
  return true;
}

bool
EXIFParser::Initialize(const uint8_t* aData, const uint32_t aLength)
{
  if (aData == nullptr)
    return false;

  // An APP1 segment larger than 64k violates the JPEG standard.
  if (aLength > 64 * 1024)
    return false;

  mStart = mCurrent = aData;
  mLength = mRemainingLength = aLength;
  mByteOrder = ByteOrder::Unknown;
  return true;
}

void
EXIFParser::Advance(const uint32_t aDistance)
{
  if (mRemainingLength >= aDistance) {
    mCurrent += aDistance;
    mRemainingLength -= aDistance;
  } else {
    mCurrent = mStart;
    mRemainingLength = 0;
  }
}

void
EXIFParser::JumpTo(const uint32_t aOffset)
{
  if (mLength >= aOffset) {
    mCurrent = mStart + aOffset;
    mRemainingLength = mLength - aOffset;
  } else {
    mCurrent = mStart;
    mRemainingLength = 0;
  }
}

bool
EXIFParser::MatchString(const char* aString, const uint32_t aLength)
{
  if (mRemainingLength < aLength)
    return false;

  for (uint32_t i = 0 ; i < aLength ; ++i) {
    if (mCurrent[i] != aString[i])
      return false;
  }

  Advance(aLength);
  return true;
}

bool
EXIFParser::MatchUInt16(const uint16_t aValue)
{
  if (mRemainingLength < 2)
    return false;
  
  const uint8_t low = aValue & 0xFF;
  const uint8_t high = aValue >> 8;

  bool matched;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      matched = mCurrent[0] == low && mCurrent[1] == high;
      break;
    case ByteOrder::BigEndian:
      matched = mCurrent[0] == high && mCurrent[1] == low;
      break;
    default:
      NS_NOTREACHED("Should know the byte order by now");
      matched = false;
  }

  if (matched)
    Advance(2);

  return matched;
}

bool
EXIFParser::ReadUInt16(uint16_t& aValue)
{
  if (mRemainingLength < 2)
    return false;
  
  bool matched = true;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      aValue = (uint32_t(mCurrent[1]) << 8) |
               (uint32_t(mCurrent[0]));
      break;
    case ByteOrder::BigEndian:
      aValue = (uint32_t(mCurrent[0]) << 8) |
               (uint32_t(mCurrent[1]));
      break;
    default:
      NS_NOTREACHED("Should know the byte order by now");
      matched = false;
  }

  if (matched)
    Advance(2);

  return matched;
}

bool
EXIFParser::ReadUInt32(uint32_t& aValue)
{
  if (mRemainingLength < 4)
    return false;
  
  bool matched = true;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      aValue = (uint32_t(mCurrent[3]) << 24) |
               (uint32_t(mCurrent[2]) << 16) |
               (uint32_t(mCurrent[1]) << 8)  |
               (uint32_t(mCurrent[0]));
      break;
    case ByteOrder::BigEndian:
      aValue = (uint32_t(mCurrent[0]) << 24) |
               (uint32_t(mCurrent[1]) << 16) |
               (uint32_t(mCurrent[2]) << 8)  |
               (uint32_t(mCurrent[3]));
      break;
    default:
      NS_NOTREACHED("Should know the byte order by now");
      matched = false;
  }

  if (matched)
    Advance(4);

  return matched;
}

} // namespace image
} // namespace mozilla
