/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_test_gtest_Common_h
#define mozilla_image_test_gtest_Common_h

#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"

class nsIInputStream;

namespace mozilla {

///////////////////////////////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////////////////////////////

struct ImageTestCase
{
  ImageTestCase(const char* aPath,
                const char* aMimeType,
                gfx::IntSize aSize,
                bool aFuzzy = false)
    : mPath(aPath)
    , mMimeType(aMimeType)
    , mSize(aSize)
    , mFuzzy(aFuzzy)
  { }

  const char* mPath;
  const char* mMimeType;
  gfx::IntSize mSize;
  bool mFuzzy;
};

struct BGRAColor
{
  BGRAColor(uint8_t aBlue, uint8_t aGreen, uint8_t aRed, uint8_t aAlpha)
    : mBlue(aBlue)
    , mGreen(aGreen)
    , mRed(aRed)
    , mAlpha(aAlpha)
  { }

  static BGRAColor Green() { return BGRAColor(0x00, 0xFF, 0x00, 0xFF); }

  uint8_t mBlue;
  uint8_t mGreen;
  uint8_t mRed;
  uint8_t mAlpha;
};


///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////

/// Loads a file from the current directory. @return an nsIInputStream for it.
already_AddRefed<nsIInputStream> LoadFile(const char* aRelativePath);

/**
 * @returns true if every pixel of @aSurface is @aColor.
 * 
 * If @aFuzzy is true, a tolerance of 1 is allowed in each color component. This
 * may be necessary for tests that involve JPEG images.
 */
bool IsSolidColor(gfx::SourceSurface* aSurface,
                  BGRAColor aColor,
                  bool aFuzzy = false);


///////////////////////////////////////////////////////////////////////////////
// Test Data
///////////////////////////////////////////////////////////////////////////////

ImageTestCase GreenPNGTestCase();
ImageTestCase GreenGIFTestCase();
ImageTestCase GreenJPGTestCase();
ImageTestCase GreenBMPTestCase();
ImageTestCase GreenICOTestCase();

ImageTestCase GreenFirstFrameAnimatedGIFTestCase();
ImageTestCase GreenFirstFrameAnimatedPNGTestCase();

ImageTestCase CorruptTestCase();

} // namespace mozilla

#endif // mozilla_image_test_gtest_Common_h
