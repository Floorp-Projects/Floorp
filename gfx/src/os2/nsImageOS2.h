/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/11/2000       IBM Corp.      Make it look more like Windows.
 */

#ifndef _nsImageOS2_h_
#define _nsImageOS2_h_

#include "nsIImage.h"

struct nsDrawingSurfaceOS2;

class nsImageOS2 : public nsIImage{
public:
  nsImageOS2();
  ~nsImageOS2();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mInfo ? mInfo->cBitCount : 0; }
  virtual PRInt32     GetHeight()         { return mInfo ? mInfo->cy : 0; }
  virtual PRBool      GetIsRowOrderTopToBottom() { return mIsTopToBottom; }
  virtual PRInt32     GetWidth()          { return mInfo ? mInfo->cx : 0; }
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual PRInt32     GetLineStride()     { return mRowBytes; }

  NS_IMETHOD     SetNaturalWidth(PRInt32 naturalwidth) { mNaturalWidth= naturalwidth; return NS_OK;}
  NS_IMETHOD     SetNaturalHeight(PRInt32 naturalheight) { mNaturalHeight= naturalheight; return NS_OK;}
  virtual PRInt32     GetNaturalWidth() {return mNaturalWidth; }
  virtual PRInt32     GetNaturalHeight() {return mNaturalHeight; }

  NS_IMETHOD          SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2);        
  virtual PRInt32     GetDecodedX1() { return mDecodedX1;}
  virtual PRInt32     GetDecodedY1() { return mDecodedY1;}
  virtual PRInt32     GetDecodedX2() { return mDecodedX2;}
  virtual PRInt32     GetDecodedY2() { return mDecodedY2;}

  virtual PRBool      GetHasAlphaMask()   { return mAlphaBits != nsnull; }

  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                      PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return mIsOptimized; }
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth()   { return mInfo ? mInfo->cx : 0; }
  virtual PRInt32     GetAlphaHeight()   { return mInfo ? mInfo->cy : 0; }
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }

  /** 
   * Draw a tiled version of the bitmap
   * @update - dwc 3/30/00
   * @param aSurface  the surface to blit to
   * @param aX The destination horizontal location
   * @param aY The destination vertical location
   * @param aWidth The destination width of the pixelmap
   * @param aHeight The destination height of the pixelmap
   * @return if TRUE, no errors
   */
// OS2TODO
  PRBool  DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,nscoord aWidth,nscoord aHeight);

  /** 
   * Return the header size of the Device Independent Bitmap(DIB).
   * @return size of header in bytes
   */
#if 0 // OS2TODO
  PRIntn      GetSizeHeader(){return sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * mNumPaletteColors;}
#endif

  /** 
   * Return the image size of the Device Independent Bitmap(DIB).
   * @update dc - 10/29/98
   * @return size of image in bytes
   */
#if 0 // OS2TODO
  PRIntn      GetSizeImage(){ return mSizeImage; }
#endif

  /** 
   * Calculate the number of bytes spaned for this image for a given width
   * @param aWidth is the width to calculate the number of bytes for
   * @return the number of bytes in this span
   */
#if 0 // OS2TODO
  PRInt32  CalcBytesSpan(PRUint32  aWidth);
#endif

  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel) {mAlphaLevel=aAlphaLevel;}

  /** 
   * Get the alpha level assigned.
   * @update dc - 10/29/98
   * @return The alpha level from 0 to 1
   */
  virtual PRInt32 GetAlphaLevel() {return(mAlphaLevel);}

  /** 
   * Get the DIB specific informations for this bitmap.
   * @update dc - 10/29/98
   * @return VOID
   */
  void* GetBitInfo()  { return mInfo; }

  NS_IMETHOD   LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   UnlockImagePixels(PRBool aMaskPixels);


 private:
  /** 
   * Clean up the memory used nsImageWin.
   * @update dc - 10/29/98
   * @param aCleanUpAll - if True, all the memory used will be released otherwise just clean up the DIB memory
   */
  void CleanUp(PRBool aCleanUpAll);

#ifdef OS2TODO
  /** 
   * Create a Device Dependent bitmap from a drawing surface
   * @update dc - 10/29/98
   * @param aSurface - The drawingsurface to create the DDB from.
   */
  void CreateDDB(nsDrawingSurface aSurface);


  /** 
   * Create a Device Dependent bitmap from a drawing surface
   * @update dc - 05/20/99
   * @param aSurface - The drawingsurface to create the DIB from.
   * @param aWidth - width of DIB
   * @param aHeight - height of DIB
   */
  nsresult ConvertDDBtoDIB(PRInt32 aWidth,PRInt32 aHeight);


  /** 
   * Print a DDB
   * @update dc - 05/20/99
   * @param aSurface - The drawingsurface to create the DIB from.
   * @param aX - x location to place image
   * @param aX - y location to place image
   * @param aWidth - width of DIB
   * @param aHeight - height of DIB
   */
  nsresult PrintDDB(nsDrawingSurface aSurface,PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight);


  /** 
   * Get an index in the palette that matches as closly as possible the passed in RGB colors
   * @update dc - 10/29/98
   * @param aR - Red component of the color to match
   * @param aG - Green component of the color to match
   * @param aB - Blue component of the color to match
   * @return - The closest palette match
   */
  PRUint8 PaletteMatch(PRUint8 r, PRUint8 g, PRUint8 b);
#endif
  BITMAPINFO2*    mInfo;
  PRBool              mIsTopToBottom;     // rows in image are top to bottom 
  HBITMAP            mBitmap;
  HBITMAP            mABitmap;
  PRUint32            mDeviceDepth;
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8*            mImageBits;         // starting address of DIB bits
  PRBool              mIsOptimized;       // Did we convert our DIB to a HBITMAP
  nsColorMap*         mColorMap;          // Redundant with mColorTable, but necessary

  PRInt32		      mNaturalWidth;
  PRInt32             mNaturalHeight;

  PRInt32             mDecodedX1;       //Keeps track of what part of image
  PRInt32             mDecodedY1;       // has been decoded.
  PRInt32             mDecodedX2; 
  PRInt32             mDecodedY2;    
    
  // alpha layer members
  PRUint8             *mAlphaBits;        // alpha layer if we made one
  PRInt8              mAlphaDepth;        // alpha layer depth
  PRInt16             mARowBytes;         // number of bytes per row in the image for tha alpha
  PRInt16             mAlphaLevel;        // an alpha level every pixel uses

  void CreateBitmaps( nsDrawingSurfaceOS2 *surf);
  void DrawBitmap( HPS hps, LONG cPts, PPOINTL pPts, LONG lRop, PRBool bMsk);
};

#endif
