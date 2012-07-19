/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_BMPHEADERS_H_
#define MOZILLA_IMAGELIB_BMPHEADERS_H_

namespace mozilla {
  namespace image {

    struct BMPFILEHEADER {
      char signature[2]; // String "BM"
      PRUint32 filesize;
      PRInt32 reserved; // Zero
      PRUint32 dataoffset; // Offset to raster data

      PRUint32 bihsize;
    };

// The length of the bitmap file header as defined in the BMP spec.
#define BFH_LENGTH 14 
// Internally we store the bitmap file header with an additional 4 bytes which
// is used to store the bitmap information header size.
#define BFH_INTERNAL_LENGTH 18

#define OS2_INTERNAL_BIH_LENGTH 8
#define WIN_V3_INTERNAL_BIH_LENGTH 36
#define WIN_V5_INTERNAL_BIH_LENGTH 120

#define OS2_BIH_LENGTH 12 // This is the real BIH size (as contained in the bihsize field of BMPFILEHEADER)
#define WIN_V3_BIH_LENGTH 40 // This is the real BIH size (as contained in the bihsize field of BMPFILEHEADER)
#define WIN_V5_BIH_LENGTH 124 // This is the real BIH size (as contained in the bihsize field of BMPFILEHEADER)

#define OS2_HEADER_LENGTH (BFH_INTERNAL_LENGTH + OS2_INTERNAL_BIH_LENGTH)
#define WIN_V3_HEADER_LENGTH (BFH_INTERNAL_LENGTH + WIN_V3_INTERNAL_BIH_LENGTH)
#define WIN_V5_HEADER_LENGTH (BFH_INTERNAL_LENGTH + WIN_V5_INTERNAL_BIH_LENGTH)

#define LCS_sRGB 0x73524742
    
    struct xyz {
      PRInt32 x, y, z;
    };

    struct xyzTriple {
      xyz r, g, b;
    };

    struct BITMAPV5HEADER {
      PRInt32 width; // Uint16 in OS/2 BMPs
      PRInt32 height; // Uint16 in OS/2 BMPs
      PRUint16 planes; // =1
      PRUint16 bpp; // Bits per pixel.
      // The rest of the header is not available in OS/2 BMP Files
      PRUint32 compression; // 0=no compression 1=8bit RLE 2=4bit RLE
      PRUint32 image_size; // (compressed) image size. Can be 0 if compression==0
      PRUint32 xppm; // Pixels per meter, horizontal
      PRUint32 yppm; // Pixels per meter, vertical
      PRUint32 colors; // Used Colors
      PRUint32 important_colors; // Number of important colors. 0=all
      PRUint32 red_mask;   // Bits used for red component
      PRUint32 green_mask; // Bits used for green component
      PRUint32 blue_mask;  // Bits used for blue component
      PRUint32 alpha_mask; // Bits used for alpha component
      PRUint32 color_space; // 0x73524742=LCS_sRGB ...
      // These members are unused unless color_space == LCS_CALIBRATED_RGB
      xyzTriple white_point; // Logical white point
      PRUint32 gamma_red;   // Red gamma component
      PRUint32 gamma_green; // Green gamma component
      PRUint32 gamma_blue;  // Blue gamma component
      PRUint32 intent; // Rendering intent
      // These members are unused unless color_space == LCS_PROFILE_*
      PRUint32 profile_offset; // Offset to profile data in bytes
      PRUint32 profile_size; // Size of profile data in bytes
      PRUint32 reserved; // =0
    };

    struct colorTable {
      PRUint8 red;
      PRUint8 green;
      PRUint8 blue;
    };

    struct bitFields {
      PRUint32 red;
      PRUint32 green;
      PRUint32 blue;
      PRUint8 redLeftShift;
      PRUint8 redRightShift;
      PRUint8 greenLeftShift;
      PRUint8 greenRightShift;
      PRUint8 blueLeftShift;
      PRUint8 blueRightShift;
    };

  } // namespace image
} // namespace mozilla

#define BITFIELD_LENGTH 12 // Length of the bitfields structure in the bmp file
#define USE_RGB

// BMPINFOHEADER.compression defines
#ifndef BI_RGB
#define BI_RGB 0
#endif
#ifndef BI_RLE8
#define BI_RLE8 1
#endif
#ifndef BI_RLE4
#define BI_RLE4 2
#endif
#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3
#endif
// BI_ALPHABITFIELDS  means no compression and specifies alpha bits
// valid only for 32bpp and 16bpp
#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS 4
#endif

// RLE Escape codes
#define RLE_ESCAPE       0
#define RLE_ESCAPE_EOL   0
#define RLE_ESCAPE_EOF   1
#define RLE_ESCAPE_DELTA 2

/// enums for mState
enum ERLEState {
  eRLEStateInitial,
  eRLEStateNeedSecondEscapeByte,
  eRLEStateNeedXDelta,
  eRLEStateNeedYDelta,    ///< mStateData will hold x delta
  eRLEStateAbsoluteMode,  ///< mStateData will hold count of existing data to read
  eRLEStateAbsoluteModePadded ///< As above, but another byte of data has to be read as padding
};

#endif
