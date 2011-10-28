/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is bitmap header definitions.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_IMAGELIB_BMPHEADERS_H_
#define MOZILLA_IMAGELIB_BMPHEADERS_H_

namespace mozilla {
  namespace imagelib {

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
#define WIN_INTERNAL_BIH_LENGTH 36

#define OS2_BIH_LENGTH 12 // This is the real BIH size (as contained in the bihsize field of BMPFILEHEADER)
#define WIN_BIH_LENGTH 40 // This is the real BIH size (as contained in the bihsize field of BMPFILEHEADER)

#define OS2_HEADER_LENGTH (BFH_INTERNAL_LENGTH + OS2_INTERNAL_BIH_LENGTH)
#define WIN_HEADER_LENGTH (BFH_INTERNAL_LENGTH + WIN_INTERNAL_BIH_LENGTH)

    struct BMPINFOHEADER {
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

  } // namespace imagelib
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