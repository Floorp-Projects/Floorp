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
      PRUint16   mReserved;
      PRUint16   mType;
      PRUint16   mCount;
    };

    struct IconDirEntry
    {
      PRUint8   mWidth;
      PRUint8   mHeight;
      PRUint8   mColorCount;
      PRUint8   mReserved;
      union {
        PRUint16 mPlanes;   // ICO
        PRUint16 mXHotspot; // CUR
      };
      union {
        PRUint16 mBitCount; // ICO
        PRUint16 mYHotspot; // CUR
      };
      PRUint32  mBytesInRes;
      PRUint32  mImageOffset;
    };


  } // namespace image
} // namespace mozilla

#endif
