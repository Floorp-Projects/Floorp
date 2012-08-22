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

    struct IconFileHeader
    {
      uint16_t   mReserved;
      uint16_t   mType;
      uint16_t   mCount;
    };

    struct IconDirEntry
    {
      uint8_t   mWidth;
      uint8_t   mHeight;
      uint8_t   mColorCount;
      uint8_t   mReserved;
      union {
        uint16_t mPlanes;   // ICO
        uint16_t mXHotspot; // CUR
      };
      union {
        uint16_t mBitCount; // ICO
        uint16_t mYHotspot; // CUR
      };
      uint32_t  mBytesInRes;
      uint32_t  mImageOffset;
    };


  } // namespace image
} // namespace mozilla

#endif
