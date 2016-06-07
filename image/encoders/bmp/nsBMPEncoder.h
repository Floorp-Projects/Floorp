/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_encoders_bmp_nsBMPEncoder_h
#define mozilla_image_encoders_bmp_nsBMPEncoder_h

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/UniquePtr.h"

#include "imgIEncoder.h"
#include "BMPHeaders.h"

#include "nsCOMPtr.h"

#define NS_BMPENCODER_CID \
{ /* 13a5320c-4c91-4FA4-bd16-b081a3ba8c0b */         \
     0x13a5320c,                                     \
     0x4c91,                                         \
     0x4fa4,                                         \
    {0xbd, 0x16, 0xb0, 0x81, 0xa3, 0Xba, 0x8c, 0x0b} \
}

namespace mozilla {
namespace image {
namespace bmp {

struct FileHeader {
  char signature[2];   // String "BM".
  uint32_t filesize;   // File size.
  int32_t reserved;    // Zero.
  uint32_t dataoffset; // Offset to raster data.
};

struct XYZ {
  int32_t x, y, z;
};

struct XYZTriple {
  XYZ r, g, b;
};

struct V5InfoHeader {
  uint32_t bihsize;          // Header size
  int32_t width;             // Uint16 in OS/2 BMPs
  int32_t height;            // Uint16 in OS/2 BMPs
  uint16_t planes;           // =1
  uint16_t bpp;              // Bits per pixel.
  uint32_t compression;      // See Compression for valid values
  uint32_t image_size;       // (compressed) image size. Can be 0 if
                             // compression==0
  uint32_t xppm;             // Pixels per meter, horizontal
  uint32_t yppm;             // Pixels per meter, vertical
  uint32_t colors;           // Used Colors
  uint32_t important_colors; // Number of important colors. 0=all
  // The rest of the header is not available in WIN_V3 BMP Files
  uint32_t red_mask;         // Bits used for red component
  uint32_t green_mask;       // Bits used for green component
  uint32_t blue_mask;        // Bits used for blue component
  uint32_t alpha_mask;       // Bits used for alpha component
  uint32_t color_space;      // 0x73524742=LCS_sRGB ...
  // These members are unused unless color_space == LCS_CALIBRATED_RGB
  XYZTriple white_point;     // Logical white point
  uint32_t gamma_red;        // Red gamma component
  uint32_t gamma_green;      // Green gamma component
  uint32_t gamma_blue;       // Blue gamma component
  uint32_t intent;           // Rendering intent
  // These members are unused unless color_space == LCS_PROFILE_*
  uint32_t profile_offset;   // Offset to profile data in bytes
  uint32_t profile_size;     // Size of profile data in bytes
  uint32_t reserved;         // =0

  static const uint32_t COLOR_SPACE_LCS_SRGB = 0x73524742;
};

} // namespace bmp
} // namespace image
} // namespace mozilla

// Provides BMP encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsBMPEncoder final : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsBMPEncoder();

protected:
  ~nsBMPEncoder();

  enum Version
  {
      VERSION_3 = 3,
      VERSION_5 = 5
  };

  // See InitData in the cpp for valid parse options
  nsresult ParseOptions(const nsAString& aOptions, Version& aVersionOut,
                        uint32_t& aBppOut);
  // Obtains data with no alpha in machine-independent byte order
  void ConvertHostARGBRow(const uint8_t* aSrc,
                          const mozilla::UniquePtr<uint8_t[]>& aDest,
                          uint32_t aPixelWidth);
  // Thread safe notify listener
  void NotifyListener();

  // Initializes the bitmap file header member mBMPFileHeader
  void InitFileHeader(Version aVersion, uint32_t aBPP, uint32_t aWidth,
                      uint32_t aHeight);
  // Initializes the bitmap info header member mBMPInfoHeader
  void InitInfoHeader(Version aVersion, uint32_t aBPP, uint32_t aWidth,
                      uint32_t aHeight);

  // Encodes the bitmap file header member mBMPFileHeader
  void EncodeFileHeader();
  // Encodes the bitmap info header member mBMPInfoHeader
  void EncodeInfoHeader();
  // Encodes a row of image data which does not have alpha data
  void EncodeImageDataRow24(const uint8_t* aData);
  // Encodes a row of image data which does have alpha data
  void EncodeImageDataRow32(const uint8_t* aData);
  // Obtains the current offset filled up to for the image buffer
  inline int32_t GetCurrentImageBufferOffset()
  {
    return static_cast<int32_t>(mImageBufferCurr - mImageBufferStart);
  }

  // These headers will always contain endian independent stuff
  // They store the BMP headers which will be encoded
  mozilla::image::bmp::FileHeader mBMPFileHeader;
  mozilla::image::bmp::V5InfoHeader mBMPInfoHeader;

  // Keeps track of the start of the image buffer
  uint8_t* mImageBufferStart;
  // Keeps track of the current position in the image buffer
  uint8_t* mImageBufferCurr;
  // Keeps track of the image buffer size
  uint32_t mImageBufferSize;
  // Keeps track of the number of bytes in the image buffer which are read
  uint32_t mImageBufferReadPoint;
  // Stores true if the image is done being encoded
  bool mFinished;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  uint32_t mNotifyThreshold;
};

#endif // mozilla_image_encoders_bmp_nsBMPEncoder_h
