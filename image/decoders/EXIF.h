/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_EXIF_H_
#define MOZILLA_IMAGELIB_EXIF_H_

#include <stdint.h>
#include "mozilla/TypedEnum.h"
#include "nsDebug.h"

#include "Orientation.h"

namespace mozilla {
namespace image {

MOZ_BEGIN_ENUM_CLASS(ByteOrder, uint8_t)
  Unknown,
  LittleEndian,
  BigEndian
MOZ_END_ENUM_CLASS(ByteOrder)

struct EXIFData
{
  EXIFData() { }
  explicit EXIFData(Orientation aOrientation) : orientation(aOrientation) { }

  const Orientation orientation;
};

class EXIFParser
{
public:
  static EXIFData
  Parse(const uint8_t* aData, const uint32_t aLength)
  {
    EXIFParser parser;
    return parser.ParseEXIF(aData, aLength);
  }

private:
  EXIFParser()
    : mStart(nullptr)
    , mCurrent(nullptr)
    , mLength(0)
    , mRemainingLength(0)
    , mByteOrder(ByteOrder::Unknown)
  { }

  EXIFData ParseEXIF(const uint8_t* aData, const uint32_t aLength);
  bool ParseEXIFHeader();
  bool ParseTIFFHeader(uint32_t& aIFD0OffsetOut);
  bool ParseIFD0(Orientation& aOrientationOut);
  bool ParseOrientation(uint16_t aType, uint32_t aCount, Orientation& aOut);

  bool Initialize(const uint8_t* aData, const uint32_t aLength);
  void Advance(const uint32_t aDistance);
  void JumpTo(const uint32_t aOffset);

  bool MatchString(const char* aString, const uint32_t aLength);
  bool MatchUInt16(const uint16_t aValue);
  bool ReadUInt16(uint16_t& aOut);
  bool ReadUInt32(uint32_t& aOut);

  const uint8_t* mStart;
  const uint8_t* mCurrent;
  uint32_t       mLength;
  uint32_t       mRemainingLength;
  ByteOrder      mByteOrder;
};

}
}

#endif
