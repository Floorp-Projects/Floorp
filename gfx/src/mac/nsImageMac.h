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
#include <QDOffscreen.h>

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
  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_TRUE; }

  virtual PRInt32     GetWidth()            { return mWidth;  }
  virtual PRInt32     GetHeight()           { return mHeight; }

  virtual PRUint8*    GetBits();
  virtual PRInt32     GetLineStride()       { return mRowBytes; }
  virtual PRBool      GetHasAlphaMask()     { return mMaskGWorld != nsnull; }

  virtual PRUint8*    GetAlphaBits();
  virtual PRInt32     GetAlphaLineStride()  { return mAlphaRowBytes; }

  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
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


  NS_IMETHOD          GetGWorldPtr(GWorldPtr* aGWorld);

  // Convert to and from the os-native PICT format. Most likely
  // used for clipboard.
  NS_IMETHOD          ConvertToPICT(PicHandle* outPicture);
  NS_IMETHOD          ConvertFromPICT(PicHandle inPicture);


  //Convert to os-native icon format(s)
  //exact format depends on the bit depth
  NS_IMETHOD          ConvertToIcon(  const nsRect& aSrcRegion, 
                                      const PRInt16 aIconDepth, 
                                      const PRInt16 aIconSize,
                                      Handle* aOutIcon,
                                      OSType* aOutIconType);

  NS_IMETHOD          ConvertAlphaToIconMask(  const nsRect& aSrcRegion, 
                                      const PRInt16 aMaskDepth, 
                                      const PRInt16 aMaskSize,
                                      Handle* aOutMask,
                                      OSType* aOutIconType);


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

  nsresult          CopyPixMap(         const Rect& aSrcRegion,
                                        const Rect& aDestRegion,
                                        const PRInt32 aDestDepth,
                                        const PRBool aCopyMaskBits,
                                        Handle *aDestData,
                                        PRBool aAllow2Bytes = PR_FALSE);

  static GDHandle   GetCachedGDeviceForDepth(PRInt32 aDepth);
  
  static OSType     MakeIconType(PRInt32 aHeight, PRInt32 aDepth, PRBool aMask);

  static OSErr      CreateGWorld(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                                        GWorldPtr* outGWorld, char** outBits, PRInt32* outRowBytes);

  static PRInt32    CalculateRowBytes(PRUint32 aWidth, PRUint32 aDepth);

  static PRUint32   GetPixelFormatForDepth(PRInt32 inDepth, PRInt32& outBitsPerPixel, CTabHandle* outDefaultColorTable = nsnull);
  
  static void       ClearGWorld(GWorldPtr);
  static OSErr      AllocateGWorld(PRInt16 depth, CTabHandle colorTable, const Rect& bounds, GWorldPtr *outGWorld);

  static nsresult   ConcatBitsHandles(  Handle srcData1, 
                                        Handle srcData2,
                                        Handle *dstData);
                                                
                                                
  static nsresult   MakeOpaqueMask( const PRInt32 aWidth,
                                        const PRInt32 aHeight,
                                        const PRInt32 aDepth,
                                        Handle *aMask);                                                
  
  static void       CopyBitsWithMask(const BitMap* srcBits, const BitMap* maskBits, PRInt16 maskDepth, const BitMap* destBits,
                            const Rect& srcRect, const Rect& maskRect, const Rect& destRect, PRBool inDrawingToPort);
  
  static PRBool     RenderingToPrinter(nsIRenderingContext &aContext);

  static OSErr      CreateGWorldInternal( PRInt32 aWidth, 
                                          PRInt32 aHeight, 
                                          PRInt32 aDepth, 
                                          GWorldPtr* outGWorld, 
                                          char** outBits,
                                          PRInt32* outRowBytes, 
                                          PRBool aAllow2Bytes);

  static PRInt32    CalculateRowBytesInternal(PRUint32 aWidth, 
                                              PRUint32 aDepth, 
                                              PRBool aAllow2Bytes);

private:

  GWorldPtr       mImageGWorld;
  char*           mImageBits;     // malloc'd block

  PRInt32         mWidth;
  PRInt32         mHeight;

  PRInt32         mRowBytes;
  PRInt32         mBytesPerPixel;
    
  // alpha layer members
  GWorldPtr       mMaskGWorld;
  char*           mMaskBits;      // malloc'd block
  
  PRInt16         mAlphaDepth;      // alpha layer depth
  PRInt32         mAlphaRowBytes;   // alpha row bytes

  PRInt32         mDecodedX1;       // Keeps track of what part of image
  PRInt32         mDecodedY1;       // has been decoded.
  PRInt32         mDecodedX2; 
  PRInt32         mDecodedY2;    
    
  //nsPoint         mLocation;      // alpha mask location

  //PRInt8          mImageCache;    // place to save off the old image for fast animation

};

#endif
