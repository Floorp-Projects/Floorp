/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "imgLoader.h"
#include "nsMimeTypes.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::image;

static void CheckMimeType(const char* aContents, size_t aLength,
                          const char* aExpected) {
  nsAutoCString detected;
  nsresult rv = imgLoader::GetMimeTypeFromContent(aContents, aLength, detected);
  if (aExpected) {
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_TRUE(detected.EqualsASCII(aExpected));
  } else {
    ASSERT_TRUE(NS_FAILED(rv));
    EXPECT_TRUE(detected.IsEmpty());
  }
}

class ImageLoader : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageLoader, DetectGIF) {
  const char buffer[] = "GIF87a";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_GIF);
}

TEST_F(ImageLoader, DetectPNG) {
  const char buffer[] = "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_PNG);
}

TEST_F(ImageLoader, DetectJPEG) {
  const char buffer[] = "\xFF\xD8\xFF";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_JPEG);
}

TEST_F(ImageLoader, DetectART) {
  const char buffer[] = "\x4A\x47\xFF\xFF\x00";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_ART);
}

TEST_F(ImageLoader, DetectBMP) {
  const char buffer[] = "BM";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_BMP);
}

TEST_F(ImageLoader, DetectICO) {
  const char buffer[] = "\x00\x00\x01\x00";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_ICO);
}

TEST_F(ImageLoader, DetectWebP) {
  const char buffer[] = "RIFF\xFF\xFF\xFF\xFFWEBPVP8L";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_WEBP);
}

TEST_F(ImageLoader, DetectAVIFMajorBrand) {
  const char buffer[] =
      "\x00\x00\x00\x20"   // box length
      "ftyp"               // box type
      "avif"               // major brand
      "\x00\x00\x00\x00"   // minor version
      "avifmif1miafMA1B";  // compatible brands
  CheckMimeType(buffer, sizeof(buffer), IMAGE_AVIF);
}

TEST_F(ImageLoader, DetectAVIFCompatibleBrand) {
  const char buffer[] =
      "\x00\x00\x00\x20"   // box length
      "ftyp"               // box type
      "XXXX"               // major brand
      "\x00\x00\x00\x00"   // minor version
      "avifmif1miafMA1B";  // compatible brands
  CheckMimeType(buffer, sizeof(buffer), IMAGE_AVIF);
}

TEST_F(ImageLoader, DetectJXLCodestream) {
  const char buffer[] = "\xff\x0a";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_JXL);
}

TEST_F(ImageLoader, DetectJXLContainer) {
  const char buffer[] =
      "\x00\x00\x00\x0c"
      "JXL "
      "\x0d\x0a\x87\x0a";
  CheckMimeType(buffer, sizeof(buffer), IMAGE_JXL);
}

TEST_F(ImageLoader, DetectNonImageMP4) {
  const char buffer[] =
      "\x00\x00\x00\x1c"  // box length
      "ftyp"              // box type
      "isom"              // major brand
      "\x00\x00\x02\x00"  // minor version
      "isomiso2mp41";     // compatible brands
  CheckMimeType(buffer, sizeof(buffer), nullptr);
}

TEST_F(ImageLoader, DetectNone) {
  const char buffer[] = "abcdefghijklmnop";
  CheckMimeType(buffer, sizeof(buffer), nullptr);
}
