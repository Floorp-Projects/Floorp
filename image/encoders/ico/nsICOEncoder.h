/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

#include "imgIEncoder.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "ICOFileHeaders.h"

class nsBMPEncoder;
class nsPNGEncoder;

#define NS_ICOENCODER_CID \
{ /*92AE3AB2-8968-41B1-8709-B6123BCEAF21 */          \
     0x92ae3ab2,                                     \
     0x8968,                                         \
     0x41b1,                                         \
    {0x87, 0x09, 0xb6, 0x12, 0x3b, 0Xce, 0xaf, 0x21} \
}

// Provides ICO encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsICOEncoder MOZ_FINAL : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsICOEncoder();
  ~nsICOEncoder();
  
  // Obtains the width of the icon directory entry
  PRUint32 GetRealWidth() const
  {
    return mICODirEntry.mWidth == 0 ? 256 : mICODirEntry.mWidth; 
  }

  // Obtains the height of the icon directory entry
  PRUint32 GetRealHeight() const
  {
    return mICODirEntry.mHeight == 0 ? 256 : mICODirEntry.mHeight; 
  }

protected:
  nsresult ParseOptions(const nsAString& aOptions, PRUint32* bpp, 
                        bool *usePNG);
  void NotifyListener();

  // Initializes the icon file header mICOFileHeader
  void InitFileHeader();
  // Initializes the icon directory info header mICODirEntry
  void InitInfoHeader(PRUint32 aBPP, PRUint8 aWidth, PRUint8 aHeight);
  // Encodes the icon file header mICOFileHeader
  void EncodeFileHeader();
  // Encodes the icon directory info header mICODirEntry
  void EncodeInfoHeader();
  // Obtains the current offset filled up to for the image buffer
  inline PRInt32 GetCurrentImageBufferOffset()
  {
    return static_cast<PRInt32>(mImageBufferCurr - mImageBufferStart);
  }

  // Holds either a PNG or a BMP depending on the encoding options specified
  // or if no encoding options specified will use the default (PNG)
  nsCOMPtr<imgIEncoder> mContainedEncoder;

  // These headers will always contain endian independent stuff.
  // Don't trust the width and height of mICODirEntry directly,
  // instead use the accessors GetRealWidth() and GetRealHeight().
  mozilla::image::IconFileHeader mICOFileHeader;
  mozilla::image::IconDirEntry mICODirEntry;

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
  // Stores true if the contained image is a PNG
  bool mUsePNG;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;
};
