/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EXIF.h"

#include "mozilla/EndianUtils.h"
#include "mozilla/StaticPrefs_image.h"

namespace mozilla::image {

// Section references in this file refer to the EXIF v2.3 standard, also known
// as CIPA DC-008-Translation-2010.

// See Section 4.6.4, Table 4.
// Typesafe enums are intentionally not used here since we're comparing to raw
// integers produced by parsing.
enum class EXIFTag : uint16_t {
  Orientation = 0x112,
  XResolution = 0x11a,
  YResolution = 0x11b,
  PixelXDimension = 0xa002,
  PixelYDimension = 0xa003,
  ResolutionUnit = 0x128,
  IFDPointer = 0x8769,
};

// See Section 4.6.2.
enum EXIFType {
  ByteType = 1,
  ASCIIType = 2,
  ShortType = 3,
  LongType = 4,
  RationalType = 5,
  UndefinedType = 7,
  SignedLongType = 9,
  SignedRational = 10,
};

static const char* EXIFHeader = "Exif\0\0";
static const uint32_t EXIFHeaderLength = 6;
static const uint32_t TIFFHeaderStart = EXIFHeaderLength;

struct ParsedEXIFData {
  Orientation orientation;
  Maybe<float> resolutionX;
  Maybe<float> resolutionY;
  Maybe<uint32_t> pixelXDimension;
  Maybe<uint32_t> pixelYDimension;
  Maybe<ResolutionUnit> resolutionUnit;
};

static float ToDppx(float aResolution, ResolutionUnit aUnit) {
  constexpr float kPointsPerInch = 72.0f;
  constexpr float kPointsPerCm = 1.0f / 2.54f;
  switch (aUnit) {
    case ResolutionUnit::Dpi:
      return aResolution / kPointsPerInch;
    case ResolutionUnit::Dpcm:
      return aResolution / kPointsPerCm;
  }
  MOZ_CRASH("Unknown resolution unit?");
}

static Resolution ResolutionFromParsedData(const ParsedEXIFData& aData,
                                           const gfx::IntSize& aRealImageSize) {
  if (!aData.resolutionUnit || !aData.resolutionX || !aData.resolutionY) {
    return {};
  }

  Resolution resolution{ToDppx(*aData.resolutionX, *aData.resolutionUnit),
                        ToDppx(*aData.resolutionY, *aData.resolutionUnit)};

  if (StaticPrefs::image_exif_density_correction_sanity_check_enabled()) {
    if (!aData.pixelXDimension || !aData.pixelYDimension) {
      return {};
    }

    const gfx::IntSize exifSize(*aData.pixelXDimension, *aData.pixelYDimension);

    gfx::IntSize scaledSize = aRealImageSize;
    resolution.ApplyTo(scaledSize.width, scaledSize.height);

    if (exifSize != scaledSize) {
      return {};
    }
  }

  return resolution;
}

/////////////////////////////////////////////////////////////
// Parse EXIF data, typically found in a JPEG's APP1 segment.
/////////////////////////////////////////////////////////////
EXIFData EXIFParser::ParseEXIF(const uint8_t* aData, const uint32_t aLength,
                               const gfx::IntSize& aRealImageSize) {
  if (!Initialize(aData, aLength)) {
    return EXIFData();
  }

  if (!ParseEXIFHeader()) {
    return EXIFData();
  }

  uint32_t offsetIFD;
  if (!ParseTIFFHeader(offsetIFD)) {
    return EXIFData();
  }

  JumpTo(offsetIFD);

  ParsedEXIFData data;
  ParseIFD(data);

  return EXIFData{data.orientation,
                  ResolutionFromParsedData(data, aRealImageSize)};
}

/////////////////////////////////////////////////////////
// Parse the EXIF header. (Section 4.7.2, Figure 30)
/////////////////////////////////////////////////////////
bool EXIFParser::ParseEXIFHeader() {
  return MatchString(EXIFHeader, EXIFHeaderLength);
}

/////////////////////////////////////////////////////////
// Parse the TIFF header. (Section 4.5.2, Table 1)
/////////////////////////////////////////////////////////
bool EXIFParser::ParseTIFFHeader(uint32_t& aIFD0OffsetOut) {
  // Determine byte order.
  if (MatchString("MM\0*", 4)) {
    mByteOrder = ByteOrder::BigEndian;
  } else if (MatchString("II*\0", 4)) {
    mByteOrder = ByteOrder::LittleEndian;
  } else {
    return false;
  }

  // Determine offset of the 0th IFD. (It shouldn't be greater than 64k, which
  // is the maximum size of the entry APP1 segment.)
  uint32_t ifd0Offset;
  if (!ReadUInt32(ifd0Offset) || ifd0Offset > 64 * 1024) {
    return false;
  }

  // The IFD offset is relative to the beginning of the TIFF header, which
  // begins after the EXIF header, so we need to increase the offset
  // appropriately.
  aIFD0OffsetOut = ifd0Offset + TIFFHeaderStart;
  return true;
}

// An arbitrary limit on the amount of pointers that we'll chase, to prevent bad
// inputs getting us stuck.
constexpr uint32_t kMaxEXIFDepth = 16;

/////////////////////////////////////////////////////////
// Parse the entries in IFD0. (Section 4.6.2)
/////////////////////////////////////////////////////////
void EXIFParser::ParseIFD(ParsedEXIFData& aData, uint32_t aDepth) {
  if (NS_WARN_IF(aDepth > kMaxEXIFDepth)) {
    return;
  }

  uint16_t entryCount;
  if (!ReadUInt16(entryCount)) {
    return;
  }

  for (uint16_t entry = 0; entry < entryCount; ++entry) {
    // Read the fields of the 12-byte entry.
    uint16_t tag;
    if (!ReadUInt16(tag)) {
      return;
    }

    uint16_t type;
    if (!ReadUInt16(type)) {
      return;
    }

    uint32_t count;
    if (!ReadUInt32(count)) {
      return;
    }

    switch (EXIFTag(tag)) {
      case EXIFTag::Orientation:
        // We should have an orientation value here; go ahead and parse it.
        if (!ParseOrientation(type, count, aData.orientation)) {
          return;
        }
        break;
      case EXIFTag::ResolutionUnit:
        if (!ParseResolutionUnit(type, count, aData.resolutionUnit)) {
          return;
        }
        break;
      case EXIFTag::XResolution:
        if (!ParseResolution(type, count, aData.resolutionX)) {
          return;
        }
        break;
      case EXIFTag::YResolution:
        if (!ParseResolution(type, count, aData.resolutionY)) {
          return;
        }
        break;
      case EXIFTag::PixelXDimension:
        if (!ParseDimension(type, count, aData.pixelXDimension)) {
          return;
        }
        break;
      case EXIFTag::PixelYDimension:
        if (!ParseDimension(type, count, aData.pixelYDimension)) {
          return;
        }
        break;
      case EXIFTag::IFDPointer: {
        uint32_t offset;
        if (!ReadUInt32(offset)) {
          return;
        }

        ScopedJump jump(*this, offset + TIFFHeaderStart);
        ParseIFD(aData, aDepth + 1);
        break;
      }

      default:
        Advance(4);
        break;
    }
  }
}

bool EXIFParser::ReadRational(float& aOut) {
  // Values larger than 4 bytes (like rationals) are specified as an offset into
  // the TIFF header.
  uint32_t valueOffset;
  if (!ReadUInt32(valueOffset)) {
    return false;
  }
  ScopedJump jumpToHeader(*this, valueOffset + TIFFHeaderStart);
  uint32_t numerator;
  if (!ReadUInt32(numerator)) {
    return false;
  }
  uint32_t denominator;
  if (!ReadUInt32(denominator)) {
    return false;
  }
  if (denominator == 0) {
    return false;
  }
  aOut = float(numerator) / float(denominator);
  return true;
}

bool EXIFParser::ParseResolution(uint16_t aType, uint32_t aCount,
                                 Maybe<float>& aOut) {
  if (!StaticPrefs::image_exif_density_correction_enabled()) {
    Advance(4);
    return true;
  }
  if (aType != RationalType || aCount != 1) {
    return false;
  }
  float value;
  if (!ReadRational(value)) {
    return false;
  }
  if (value == 0.0f) {
    return false;
  }
  aOut = Some(value);
  return true;
}

bool EXIFParser::ParseDimension(uint16_t aType, uint32_t aCount,
                                Maybe<uint32_t>& aOut) {
  if (!StaticPrefs::image_exif_density_correction_enabled()) {
    Advance(4);
    return true;
  }

  if (aCount != 1) {
    return false;
  }

  switch (aType) {
    case ShortType: {
      uint16_t value;
      if (!ReadUInt16(value)) {
        return false;
      }
      aOut = Some(value);
      Advance(2);
      break;
    }
    case LongType: {
      uint32_t value;
      if (!ReadUInt32(value)) {
        return false;
      }
      aOut = Some(value);
      break;
    }
    default:
      return false;
  }
  return true;
}

bool EXIFParser::ParseResolutionUnit(uint16_t aType, uint32_t aCount,
                                     Maybe<ResolutionUnit>& aOut) {
  if (!StaticPrefs::image_exif_density_correction_enabled()) {
    Advance(4);
    return true;
  }
  if (aType != ShortType || aCount != 1) {
    return false;
  }
  uint16_t value;
  if (!ReadUInt16(value)) {
    return false;
  }
  switch (value) {
    case 2:
      aOut = Some(ResolutionUnit::Dpi);
      break;
    case 3:
      aOut = Some(ResolutionUnit::Dpcm);
      break;
    default:
      return false;
  }

  // This is a 32-bit field, but the unit value only occupies the first 16 bits.
  // We need to advance another 16 bits to consume the entire field.
  Advance(2);
  return true;
}

bool EXIFParser::ParseOrientation(uint16_t aType, uint32_t aCount,
                                  Orientation& aOut) {
  // Sanity check the type and count.
  if (aType != ShortType || aCount != 1) {
    return false;
  }

  uint16_t value;
  if (!ReadUInt16(value)) {
    return false;
  }

  switch (value) {
    case 1:
      aOut = Orientation(Angle::D0, Flip::Unflipped);
      break;
    case 2:
      aOut = Orientation(Angle::D0, Flip::Horizontal);
      break;
    case 3:
      aOut = Orientation(Angle::D180, Flip::Unflipped);
      break;
    case 4:
      aOut = Orientation(Angle::D180, Flip::Horizontal);
      break;
    case 5:
      aOut = Orientation(Angle::D90, Flip::Horizontal);
      break;
    case 6:
      aOut = Orientation(Angle::D90, Flip::Unflipped);
      break;
    case 7:
      aOut = Orientation(Angle::D270, Flip::Horizontal);
      break;
    case 8:
      aOut = Orientation(Angle::D270, Flip::Unflipped);
      break;
    default:
      return false;
  }

  // This is a 32-bit field, but the orientation value only occupies the first
  // 16 bits. We need to advance another 16 bits to consume the entire field.
  Advance(2);
  return true;
}

bool EXIFParser::Initialize(const uint8_t* aData, const uint32_t aLength) {
  if (aData == nullptr) {
    return false;
  }

  // An APP1 segment larger than 64k violates the JPEG standard.
  if (aLength > 64 * 1024) {
    return false;
  }

  mStart = mCurrent = aData;
  mLength = mRemainingLength = aLength;
  mByteOrder = ByteOrder::Unknown;
  return true;
}

void EXIFParser::Advance(const uint32_t aDistance) {
  if (mRemainingLength >= aDistance) {
    mCurrent += aDistance;
    mRemainingLength -= aDistance;
  } else {
    mCurrent = mStart;
    mRemainingLength = 0;
  }
}

void EXIFParser::JumpTo(const uint32_t aOffset) {
  if (mLength >= aOffset) {
    mCurrent = mStart + aOffset;
    mRemainingLength = mLength - aOffset;
  } else {
    mCurrent = mStart;
    mRemainingLength = 0;
  }
}

bool EXIFParser::MatchString(const char* aString, const uint32_t aLength) {
  if (mRemainingLength < aLength) {
    return false;
  }

  for (uint32_t i = 0; i < aLength; ++i) {
    if (mCurrent[i] != aString[i]) {
      return false;
    }
  }

  Advance(aLength);
  return true;
}

bool EXIFParser::MatchUInt16(const uint16_t aValue) {
  if (mRemainingLength < 2) {
    return false;
  }

  bool matched;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      matched = LittleEndian::readUint16(mCurrent) == aValue;
      break;
    case ByteOrder::BigEndian:
      matched = BigEndian::readUint16(mCurrent) == aValue;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should know the byte order by now");
      matched = false;
  }

  if (matched) {
    Advance(2);
  }

  return matched;
}

bool EXIFParser::ReadUInt16(uint16_t& aValue) {
  if (mRemainingLength < 2) {
    return false;
  }

  bool matched = true;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      aValue = LittleEndian::readUint16(mCurrent);
      break;
    case ByteOrder::BigEndian:
      aValue = BigEndian::readUint16(mCurrent);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should know the byte order by now");
      matched = false;
  }

  if (matched) {
    Advance(2);
  }

  return matched;
}

bool EXIFParser::ReadUInt32(uint32_t& aValue) {
  if (mRemainingLength < 4) {
    return false;
  }

  bool matched = true;
  switch (mByteOrder) {
    case ByteOrder::LittleEndian:
      aValue = LittleEndian::readUint32(mCurrent);
      break;
    case ByteOrder::BigEndian:
      aValue = BigEndian::readUint32(mCurrent);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should know the byte order by now");
      matched = false;
  }

  if (matched) {
    Advance(4);
  }

  return matched;
}

}  // namespace mozilla::image
