/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_BMPFileHeaders_h
#define mozilla_image_BMPFileHeaders_h

#include <stddef.h>
#include <stdint.h>

namespace mozilla {
namespace image {
namespace bmp {

// This length is stored in the |bihsize| field of bmp::FileHeader.
struct InfoHeaderLength {
  enum {
    WIN_V2 = 12,
    WIN_V3 = 40,
    WIN_V4 = 108,
    WIN_V5 = 124,

    // OS2_V1 is omitted; it's the same as WIN_V2.
    OS2_V2_MIN = 16,    // Minimum allowed value for OS2v2.
    OS2_V2_MAX = 64,    // Maximum allowed value for OS2v2.
  };
};

struct FileHeader {
  char signature[2];   // String "BM".
  uint32_t filesize;   // File size; unreliable in practice.
  int32_t reserved;    // Zero.
  uint32_t dataoffset; // Offset to raster data.

  // The length of the file header as defined in the BMP spec.
  static const size_t LENGTH = 14;
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
  // The rest of the header is not available in WIN_V2/OS2_V1 BMP Files
  uint32_t compression;      // See Compression for valid values
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

#endif // mozilla_image_BMPFileHeaders_h
