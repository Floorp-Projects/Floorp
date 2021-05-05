/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_EXIF_h
#define mozilla_image_decoders_EXIF_h

#include <stdint.h>
#include "nsDebug.h"

#include "Orientation.h"
#include "mozilla/Maybe.h"
#include "mozilla/image/Resolution.h"
#include "mozilla/gfx/Point.h"

namespace mozilla::image {

enum class ByteOrder : uint8_t { Unknown, LittleEndian, BigEndian };

struct EXIFData {
  const Orientation orientation = Orientation();
  const Resolution resolution = Resolution();
};

struct ParsedEXIFData;

enum class ResolutionUnit : uint8_t {
  Dpi,
  Dpcm,
};

class EXIFParser {
 public:
  static EXIFData Parse(const uint8_t* aData, const uint32_t aLength,
                        const gfx::IntSize& aRealImageSize) {
    EXIFParser parser;
    return parser.ParseEXIF(aData, aLength, aRealImageSize);
  }

 private:
  EXIFParser()
      : mStart(nullptr),
        mCurrent(nullptr),
        mLength(0),
        mRemainingLength(0),
        mByteOrder(ByteOrder::Unknown) {}

  EXIFData ParseEXIF(const uint8_t* aData, const uint32_t aLength,
                     const gfx::IntSize& aRealImageSize);
  bool ParseEXIFHeader();
  bool ParseTIFFHeader(uint32_t& aIFD0OffsetOut);

  void ParseIFD(ParsedEXIFData&, uint32_t aDepth = 0);
  bool ParseOrientation(uint16_t aType, uint32_t aCount, Orientation&);
  bool ParseResolution(uint16_t aType, uint32_t aCount, Maybe<float>&);
  bool ParseResolutionUnit(uint16_t aType, uint32_t aCount,
                           Maybe<ResolutionUnit>&);
  bool ParseDimension(uint16_t aType, uint32_t aCount, Maybe<uint32_t>&);

  bool Initialize(const uint8_t* aData, const uint32_t aLength);
  void Advance(const uint32_t aDistance);
  void JumpTo(const uint32_t aOffset);

  uint32_t CurrentOffset() const { return mCurrent - mStart; }

  class ScopedJump {
    EXIFParser& mParser;
    uint32_t mOldOffset;

   public:
    ScopedJump(EXIFParser& aParser, uint32_t aOffset)
        : mParser(aParser), mOldOffset(aParser.CurrentOffset()) {
      mParser.JumpTo(aOffset);
    }

    ~ScopedJump() { mParser.JumpTo(mOldOffset); }
  };

  bool MatchString(const char* aString, const uint32_t aLength);
  bool MatchUInt16(const uint16_t aValue);
  bool ReadUInt16(uint16_t& aOut);
  bool ReadUInt32(uint32_t& aOut);
  bool ReadRational(float& aOut);

  const uint8_t* mStart;
  const uint8_t* mCurrent;
  uint32_t mLength;
  uint32_t mRemainingLength;
  ByteOrder mByteOrder;
};

}  // namespace mozilla::image

#endif  // mozilla_image_decoders_EXIF_h
