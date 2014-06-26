/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef MOZILLA_IMAGELIB_ICOHEADERS_H_
#define MOZILLA_IMAGELIB_ICOHEADERS_H_

namespace mozilla {
  namespace image {

    #define ICONFILEHEADERSIZE 6
    #define ICODIRENTRYSIZE 16
    #define PNGSIGNATURESIZE 8
    #define BMPFILEHEADERSIZE 14

    /**
     * The header that comes right at the start of an icon file. (This
     * corresponds to the Windows ICONDIR structure.)
     */
    struct IconFileHeader
    {
      /**
       * Must be set to 0;
       */
      uint16_t   mReserved;
      /**
       * 1 for icon (.ICO) image (or 2 for cursor (.CUR) image (icon with the
       * addition of a hotspot), but we don't support cursor).
       */
      uint16_t   mType;
      /**
       * The number of BMP/PNG images contained in the icon file.
       */
      uint16_t   mCount;
    };

    /**
     * For each BMP/PNG image that the icon file containts there must be a
     * corresponding icon dir entry. (This corresponds to the Windows
     * ICONDIRENTRY structure.) These entries are encoded directly after the
     * IconFileHeader.
     */
    struct IconDirEntry
    {
      uint8_t   mWidth;
      uint8_t   mHeight;
      /**
       * The number of colors in the color palette of the BMP/PNG that this dir
       * entry corresponds to, or 0 if the image does not use a color palette.
       */
      uint8_t   mColorCount;
      /**
       * Should be set to 0.
       */
      uint8_t   mReserved;
      union {
        uint16_t mPlanes;   // ICO
        uint16_t mXHotspot; // CUR
      };
      union {
        uint16_t mBitCount; // ICO (bits per pixel)
        uint16_t mYHotspot; // CUR
      };
      /**
       * "bytes in resource" is the length of the encoded BMP/PNG that this dir
       * entry corresponds to.
       */
      uint32_t  mBytesInRes;
      /**
       * The offset of the start of the encoded BMP/PNG that this dir entry
       * corresponds to (from the start of the icon file).
       */
      uint32_t  mImageOffset;
    };


  } // namespace image
} // namespace mozilla

#endif
