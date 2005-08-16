/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsImageMac_h___
#define nsImageMac_h___

#include "nsIImage.h"
#include "nsIImageMac.h"

class nsImageMac : public nsIImage, public nsIImageMac
{
public:
                      nsImageMac();
  virtual             ~nsImageMac();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRInt32     GetBytesPix()         { return mBytesPerPixel; }    // this is unused
  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_FALSE; }

  virtual PRInt32     GetWidth()            { return mWidth;  }
  virtual PRInt32     GetHeight()           { return mHeight; }

  virtual PRUint8*    GetBits()             { return mImageBits; }
  virtual PRInt32     GetLineStride()       { return mRowBytes; }
  virtual PRBool      GetHasAlphaMask()     { return mAlphaBits != nsnull; }

  virtual PRUint8*    GetAlphaBits()        { return mAlphaBits; }
  virtual PRInt32     GetAlphaLineStride()  { return mAlphaRowBytes; }

  // Called when an image decoder updates the image bits (mImageBits &
  // mAlphaBits).  'aFlags' is ignored.
  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags,
                                   nsRect *aUpdateRect);
  virtual PRBool      GetIsImageComplete();

  // Optimizes memory usage for object.
  virtual nsresult    Optimize(nsIDeviceContext* aContext);

  virtual nsColorMap* GetColorMap()         { return nsnull; }

  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                              PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                              PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                              PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  NS_IMETHOD          DrawTile(nsIRenderingContext &aContext,
                              nsIDrawingSurface* aSurface,
                              PRInt32 aSXOffset, PRInt32 aSYOffset,
                              PRInt32 aPadX, PRInt32 aPadY,
                              const nsRect &aTileRect);


   /**
    * Get the alpha depth for the image mask
    * @update - lordpixel 2001/05/16
    * @return  the alpha mask depth for the image, ie, 0, 1 or 8
    */
  virtual PRInt8      GetAlphaDepth() { return mAlphaDepth; }

  NS_IMETHOD          DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
                                  nscoord aDWidth, nscoord aDHeight);

  virtual void*       GetBitInfo()          { return nsnull; }

  NS_IMETHOD          LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD          UnlockImagePixels(PRBool aMaskPixels);


  // Convert to and from the os-native PICT format. Most likely
  // used for clipboard.
  NS_IMETHOD          ConvertToPICT(PicHandle* outPicture);
  NS_IMETHOD          ConvertFromPICT(PicHandle inPicture);


protected:

  nsresult          SlowTile(nsIRenderingContext &aContext,
                                        nsIDrawingSurface* aSurface,
                                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                                        PRInt32 aPadX, PRInt32 aPadY,
                                        const nsRect &aTileRect);

  nsresult          DrawTileQuickly(nsIRenderingContext &aContext,
                                        nsIDrawingSurface* aSurface,
                                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                                        const nsRect &aTileRect);

  nsresult          DrawTileWithQuartz(nsIDrawingSurface* aSurface,
                                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                                        PRInt32 aPadX, PRInt32 aPadY,
                                        const nsRect &aTileRect);
  
  static PRInt32    CalculateRowBytes(PRUint32 aWidth, PRUint32 aDepth);

  inline static PRInt32 CalculateRowBytesInternal(PRUint32 aWidth,
                                                  PRUint32 aDepth,
                                                  PRBool aAllow2Bytes);

  static PRBool     RenderingToPrinter(nsIRenderingContext &aContext);

  // Recreate internal image structure from updated image bits.
  nsresult          EnsureCachedImage();

  // Get/Set/Clear alpha bit at position 'x' in the row pointed to by 'rowptr'.
  inline PRUint8 GetAlphaBit(PRUint8* rowptr, PRUint32 x) {
    return (rowptr[x >> 3] & (1 << (7 - x & 0x7)));
  }
  inline void SetAlphaBit(PRUint8* rowptr, PRUint32 x) {
    rowptr[x >> 3] |= (1 << (7 - x & 0x7));
  }
  inline void ClearAlphaBit(PRUint8* rowptr, PRUint32 x) {
    rowptr[x >> 3] &= ~(1 << (7 - x & 0x7));
  }

  // Takes ownership of the given image and bitmap.  The CGImageRef is retained.
  void AdoptImage(CGImageRef aNewImage, PRUint8* aNewBitamp);

private:

  PRUint8*        mImageBits;     // malloc'd block
  CGImageRef      mImage;

  PRInt32         mWidth;
  PRInt32         mHeight;

  PRInt32         mRowBytes;
  PRInt32         mBytesPerPixel;

  // alpha layer members
  PRUint8*        mAlphaBits;      // malloc'd block

  PRInt32         mAlphaRowBytes;   // alpha row bytes
  PRInt8          mAlphaDepth;      // alpha layer depth

  PRPackedBool    mPendingUpdate;   // true when we need to recreate CGImageRef
  PRPackedBool    mOptimized;       // true when nsImage object has been
                                    // optimized (see |Optimize()|)

  PRInt32         mDecodedX1;       // Keeps track of what part of image
  PRInt32         mDecodedY1;       // has been decoded.
  PRInt32         mDecodedX2;
  PRInt32         mDecodedY2;
};

#endif
