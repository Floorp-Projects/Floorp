/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

#include "imgIEncoder.h"
#include "BMPFileHeaders.h"

#include "nsCOMPtr.h"

#define NS_BMPENCODER_CID \
{ /* 13a5320c-4c91-4FA4-bd16-b081a3ba8c0b */         \
     0x13a5320c,                                     \
     0x4c91,                                         \
     0x4fa4,                                         \
    {0xbd, 0x16, 0xb0, 0x81, 0xa3, 0Xba, 0x8c, 0x0b} \
}

// Provides BMP encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsBMPEncoder MOZ_FINAL : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsBMPEncoder();
  ~nsBMPEncoder();

protected:
  // See InitData in the cpp for valid parse options
  nsresult ParseOptions(const nsAString& aOptions, PRUint32* bpp);
  // Obtains data with no alpha in machine-independent byte order
  void ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                          PRUint32 aPixelWidth);
  // Strips the alpha from aSrc and puts it in aDest
  void StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
                  PRUint32 aPixelWidth);
  // Thread safe notify listener
  void NotifyListener();

  // Initializes the bitmap file header member mBMPFileHeader
  void InitFileHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight);
  // Initializes the bitmap info header member mBMPInfoHeader
  void InitInfoHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight);
  // Encodes the bitmap file header member mBMPFileHeader
  void EncodeFileHeader();
  // Encodes the bitmap info header member mBMPInfoHeader
  void EncodeInfoHeader();
  // Encodes a row of image data which does not have alpha data
  void EncodeImageDataRow24(const PRUint8* aData);
  // Encodes a row of image data which does have alpha data
  void EncodeImageDataRow32(const PRUint8* aData);
  // Obtains the current offset filled up to for the image buffer
  inline PRInt32 GetCurrentImageBufferOffset()
  {
    return static_cast<PRInt32>(mImageBufferCurr - mImageBufferStart);
  }

  // These headers will always contain endian independent stuff 
  // They store the BMP headers which will be encoded
  mozilla::image::BMPFILEHEADER mBMPFileHeader;
  mozilla::image::BMPINFOHEADER mBMPInfoHeader;

  // Keeps track of the start of the image buffer
  PRUint8* mImageBufferStart;
  // Keeps track of the current position in the image buffer
  PRUint8* mImageBufferCurr;
  // Keeps track of the image buffer size
  PRUint32 mImageBufferSize;
  // Keeps track of the number of bytes in the image buffer which are read
  PRUint32 mImageBufferReadPoint;
  // Stores true if the image is done being encoded
  bool mFinished;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;
};
