/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsICODecoder_h
#define _nsICODecoder_h

#include "nsAutoPtr.h"
#include "Decoder.h"
#include "imgIDecoderObserver.h"
#include "nsBMPDecoder.h"
#include "nsPNGDecoder.h"
#include "ICOFileHeaders.h"

namespace mozilla {
namespace image {

class RasterImage;

class nsICODecoder : public Decoder
{
public:

  nsICODecoder(RasterImage &aImage, imgIDecoderObserver* aObserver);
  virtual ~nsICODecoder();

  // Obtains the width of the icon directory entry
  PRUint32 GetRealWidth() const
  {
    return mDirEntry.mWidth == 0 ? 256 : mDirEntry.mWidth; 
  }

  // Obtains the height of the icon directory entry
  PRUint32 GetRealHeight() const
  {
    return mDirEntry.mHeight == 0 ? 256 : mDirEntry.mHeight; 
  }

  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();

private:
  // Writes to the contained decoder and sets the appropriate errors
  // Returns true if there are no errors.
  bool WriteToContainedDecoder(const char* aBuffer, PRUint32 aCount);

  // Processes a single dir entry of the icon resource
  void ProcessDirEntry(IconDirEntry& aTarget);
  // Sets the hotspot property of if we have a cursor
  void SetHotSpotIfCursor();
  // Creates a bitmap file header buffer, returns true if successful
  bool FillBitmapFileHeaderBuffer(PRInt8 *bfh);
  // Fixes the ICO height to match that of the BIH.
  // and also fixes the BIH height to be /2 of what it was.
  // See definition for explanation.
  // Returns false if invalid information is contained within.
  bool FixBitmapHeight(PRInt8 *bih);
  // Fixes the ICO width to match that of the BIH.
  // Returns false if invalid information is contained within.
  bool FixBitmapWidth(PRInt8 *bih);
  // Extract bitmap info header size count from BMP information header
  PRInt32 ExtractBIHSizeFromBitmap(PRInt8 *bih);
  // Extract bit count from BMP information header
  PRInt32 ExtractBPPFromBitmap(PRInt8 *bih);
  // Calculates the row size in bytes for the AND mask table
  PRUint32 CalcAlphaRowSize();
  // Obtains the number of colors from the BPP, mBPP must be filled in
  PRUint16 GetNumColors();

  PRUint16 mBPP; // Stores the images BPP
  PRUint32 mPos; // Keeps track of the position we have decoded up until
  PRUint16 mNumIcons; // Stores the number of icons in the ICO file
  PRUint16 mCurrIcon; // Stores the current dir entry index we are processing
  PRUint32 mImageOffset; // Stores the offset of the image data we want
  PRUint8 *mRow;      // Holds one raw line of the image
  PRInt32 mCurLine;   // Line index of the image that's currently being decoded
  PRUint32 mRowBytes; // How many bytes of the row were already received
  PRInt32 mOldLine;   // Previous index of the line 
  nsAutoPtr<Decoder> mContainedDecoder; // Contains either a BMP or PNG resource

  char mDirEntryArray[ICODIRENTRYSIZE]; // Holds the current dir entry buffer
  IconDirEntry mDirEntry; // Holds a decoded dir entry
  // Holds the potential bytes that can be a PNG signature
  char mSignature[PNGSIGNATURESIZE]; 
  // Holds the potential bytes for a bitmap information header
  char mBIHraw[40];
  // Stores whether or not the icon file we are processing has type 1 (icon)
  bool mIsCursor;
  // Stores whether or not the contained resource is a PNG
  bool mIsPNG;
};

} // namespace image
} // namespace mozilla

#endif
