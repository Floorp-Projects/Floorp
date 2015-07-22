/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_BMPFileHeaders_h
#define mozilla_image_BMPFileHeaders_h

#include <stddef.h>
#include <stdint.h>

namespace mozilla {
namespace image {

// This is the real BIH size (as contained in the bihsize field of
// BMPFILEHEADER).
struct BIH_LENGTH {
  enum {
    OS2 = 12,
    WIN_V3 = 40,
    WIN_V5 = 124
  };
};

struct BIH_INTERNAL_LENGTH {
  enum {
    OS2 = 8,
    WIN_V3 = 36,
    WIN_V5 = 120
  };
};

struct BMPFILEHEADER {
  char signature[2];   // String "BM"
  uint32_t filesize;
  int32_t reserved;    // Zero
  uint32_t dataoffset; // Offset to raster data

  uint32_t bihsize;

  // The length of the bitmap file header as defined in the BMP spec.
  static const size_t LENGTH = 14;

  // Internally we store the bitmap file header with an additional 4 bytes which
  // is used to store the bitmap information header size.
  static const size_t INTERNAL_LENGTH = 18;
};

struct BMP_HEADER_LENGTH {
  enum {
    OS2 = BMPFILEHEADER::INTERNAL_LENGTH + BIH_INTERNAL_LENGTH::OS2,
    WIN_V3 = BMPFILEHEADER::INTERNAL_LENGTH + BIH_INTERNAL_LENGTH::WIN_V3,
    WIN_V5 = BMPFILEHEADER::INTERNAL_LENGTH + BIH_INTERNAL_LENGTH::WIN_V5
  };
};

struct xyz {
  int32_t x, y, z;
};

struct xyzTriple {
  xyz r, g, b;
};

struct BITMAPV5HEADER {
  int32_t width;             // Uint16 in OS/2 BMPs
  int32_t height;            // Uint16 in OS/2 BMPs
  uint16_t planes;           // =1
  uint16_t bpp;              // Bits per pixel.
  // The rest of the header is not available in OS/2 BMP Files
  uint32_t compression;      // 0=no compression 1=8bit RLE 2=4bit RLE
  uint32_t image_size;       // (compressed) image size. Can be 0 if
                             // compression==0
  uint32_t xppm;             // Pixels per meter, horizontal
  uint32_t yppm;             // Pixels per meter, vertical
  uint32_t colors;           // Used Colors
  uint32_t important_colors; // Number of important colors. 0=all
  uint32_t red_mask;         // Bits used for red component
  uint32_t green_mask;       // Bits used for green component
  uint32_t blue_mask;        // Bits used for blue component
  uint32_t alpha_mask;       // Bits used for alpha component
  uint32_t color_space;      // 0x73524742=LCS_sRGB ...
  // These members are unused unless color_space == LCS_CALIBRATED_RGB
  xyzTriple white_point;     // Logical white point
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

struct colorTable {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct bitFields {
  uint32_t red;
  uint32_t green;
  uint32_t blue;
  uint8_t redLeftShift;
  uint8_t redRightShift;
  uint8_t greenLeftShift;
  uint8_t greenRightShift;
  uint8_t blueLeftShift;
  uint8_t blueRightShift;

  // Length of the bitfields structure in the BMP file.
  static const size_t LENGTH = 12;
};

struct BMPINFOHEADER {
  // BMPINFOHEADER.compression definitions.
  enum {
    RGB = 0,
    RLE8 = 1,
    RLE4 = 2,
    BITFIELDS = 3,

    // ALPHABITFIELDS means no compression and specifies alpha bits. Valid only
    // for 32bpp and 16bpp.
    ALPHABITFIELDS = 4
  };
};

// RLE escape codes.
struct RLE {
  enum {
    ESCAPE = 0,
    ESCAPE_EOL = 0,
    ESCAPE_EOF = 1,
    ESCAPE_DELTA = 2
  };
};

/// enums for mState
enum ERLEState {
  eRLEStateInitial,
  eRLEStateNeedSecondEscapeByte,
  eRLEStateNeedXDelta,
  eRLEStateNeedYDelta,        ///< mStateData will hold x delta
  eRLEStateAbsoluteMode,      ///< mStateData will hold count of existing data
                              ///< to read
  eRLEStateAbsoluteModePadded ///< As above, but another byte of data has to
                              ///< be read as padding
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_BMPFileHeaders_h
