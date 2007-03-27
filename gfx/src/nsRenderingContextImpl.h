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

#ifndef nsRenderingContextImpl_h___
#define nsRenderingContextImpl_h___

#include "gfxCore.h"
#include "nsIRenderingContext.h"
#include "nsPoint.h"
#include "nsSize.h"

#ifdef MOZ_CAIRO_GFX
class gfxContext;
#endif

typedef struct {	
    double x;	  // x coordinate of edge's intersection with current scanline */
    double dx;	// change in x with respect to y 
    int i;	    // edge number: edge i goes from mPointList[i] to mPointList[i+1] 
} Edge;

class nsRenderingContextImpl : public nsIRenderingContext
{

// CLASS MEMBERS
public:


protected:
  nsTransform2D		  *mTranMatrix;				// The rendering contexts transformation matrix
  int               mAct;		        		// number of active edges 
  Edge              *mActive;	      		// active edge list:edges crossing scanline y

public:
  nsRenderingContextImpl();


// CLASS METHODS

  // See the description in nsIRenderingContext.h
  NS_IMETHOD FlushRect(const nsRect& aRect);
  NS_IMETHOD FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  /**
   * Gets the Pen Mode for the RenderingContext
   * @param aPenMode The Pen Mode to be retrieved
   * @return NS_OK if the Pen Mode is correctly retrieved
   */
  NS_IMETHOD GetPenMode(nsPenMode &aPenMode) { return NS_ERROR_FAILURE;}

  virtual void* GetNativeGraphicData(nsIRenderingContext::GraphicDataType aType)
  { return nsnull; }

  /**
   * Sets the Pen Mode for the RenderingContext 
   * @param aPenMode The Pen Mode
   * @return NS_OK if the Pen Mode is correctly set
   */
  NS_IMETHOD SetPenMode(nsPenMode aPenMode) { return NS_ERROR_FAILURE;}
  
  NS_IMETHOD PushTranslation(PushedTranslation* aState);
  NS_IMETHOD PopTranslation(PushedTranslation* aState);
  NS_IMETHOD SetTranslation(nscoord aX, nscoord aY);

  /**
   * Return the maximum length of a string that can be handled by the platform
   * using the current font metrics.
   * The implementation here is just a stub; classes that don't override
   * the safe string methods need to implement this.
   */
  virtual PRInt32 GetMaxStringLength() { return 1; }

  /**
   * Let the device context know whether we want text reordered with
   * right-to-left base direction
   */
  NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);
  NS_IMETHOD GetRightToLeftText(PRBool* aIsRTL);

#ifndef MOZ_CAIRO_GFX
  NS_IMETHOD DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect);
#endif
  NS_IMETHOD DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, const nsRect * aTargetRect);

  NS_IMETHOD GetClusterInfo(const PRUnichar *aText,
                            PRUint32 aLength,
                            PRUint8 *aClusterStarts);
  virtual PRInt32 GetPosition(const PRUnichar *aText,
                              PRUint32 aLength,
                              nsPoint aPt);

  NS_IMETHOD GetRangeWidth(const PRUnichar *aText,
                           PRUint32 aLength,
                           PRUint32 aStart,
                           PRUint32 aEnd,
                           PRUint32 &aWidth);
  NS_IMETHOD GetRangeWidth(const char *aText,
                           PRUint32 aLength,
                           PRUint32 aStart,
                           PRUint32 aEnd,
                           PRUint32 &aWidth);

  // Silence C++ hiding warnings
  NS_IMETHOD GetWidth(char aC, nscoord &aWidth) = 0;
  NS_IMETHOD GetWidth(PRUnichar aC, nscoord &aWidth,
                      PRInt32 *aFontID = nsnull) = 0;

  // Safe string method variants: by default, these defer to the more
  // elaborate methods below
  NS_IMETHOD GetWidth(const nsString& aString, nscoord &aWidth,
                      PRInt32 *aFontID = nsnull);
  NS_IMETHOD GetWidth(const char* aString, nscoord& aWidth);
  NS_IMETHOD DrawString(const nsString& aString, nscoord aX, nscoord aY,
                        PRInt32 aFontID = -1,
                        const nscoord* aSpacing = nsnull);

  // Safe string methods
  NS_IMETHOD GetWidth(const char* aString, PRUint32 aLength,
                      nscoord& aWidth);
  NS_IMETHOD GetWidth(const PRUnichar *aString, PRUint32 aLength,
                      nscoord &aWidth, PRInt32 *aFontID = nsnull);

  NS_IMETHOD GetTextDimensions(const char* aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions);
  NS_IMETHOD GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions, PRInt32* aFontID = nsnull);

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
  NS_IMETHOD GetTextDimensions(const char*       aString,
                               PRInt32           aLength,
                               PRInt32           aAvailWidth,
                               PRInt32*          aBreaks,
                               PRInt32           aNumBreaks,
                               nsTextDimensions& aDimensions,
                               PRInt32&          aNumCharsFit,
                               nsTextDimensions& aLastWordDimensions,
                               PRInt32*          aFontID = nsnull);

  NS_IMETHOD GetTextDimensions(const PRUnichar*  aString,
                               PRInt32           aLength,
                               PRInt32           aAvailWidth,
                               PRInt32*          aBreaks,
                               PRInt32           aNumBreaks,
                               nsTextDimensions& aDimensions,
                               PRInt32&          aNumCharsFit,
                               nsTextDimensions& aLastWordDimensions,
                               PRInt32*          aFontID = nsnull);
#endif
#ifdef MOZ_MATHML
  NS_IMETHOD
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
  NS_IMETHOD
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics,
                     PRInt32*           aFontID = nsnull);
#endif
  NS_IMETHOD DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing = nsnull);
  NS_IMETHOD DrawString(const PRUnichar *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        PRInt32 aFontID = -1,
                        const nscoord* aSpacing = nsnull);

  // Unsafe platform-specific implementations
  NS_IMETHOD GetWidthInternal(const char* aString, PRUint32 aLength,
                              nscoord& aWidth)
  { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetWidthInternal(const PRUnichar *aString, PRUint32 aLength,
                              nscoord &aWidth, PRInt32 *aFontID = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD GetTextDimensionsInternal(const char* aString, PRUint32 aLength,
                                       nsTextDimensions& aDimensions)
  { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetTextDimensionsInternal(const PRUnichar* aString, PRUint32 aLength,
                                       nsTextDimensions& aDimensions, PRInt32* aFontID = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
  NS_IMETHOD GetTextDimensionsInternal(const char*       aString,
                                       PRInt32           aLength,
                                       PRInt32           aAvailWidth,
                                       PRInt32*          aBreaks,
                                       PRInt32           aNumBreaks,
                                       nsTextDimensions& aDimensions,
                                       PRInt32&          aNumCharsFit,
                                       nsTextDimensions& aLastWordDimensions,
                                       PRInt32*          aFontID = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD GetTextDimensionsInternal(const PRUnichar*  aString,
                                       PRInt32           aLength,
                                       PRInt32           aAvailWidth,
                                       PRInt32*          aBreaks,
                                       PRInt32           aNumBreaks,
                                       nsTextDimensions& aDimensions,
                                       PRInt32&          aNumCharsFit,
                                       nsTextDimensions& aLastWordDimensions,
                                       PRInt32*          aFontID = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }
#endif
#ifdef MOZ_MATHML
  NS_IMETHOD
  GetBoundingMetricsInternal(const char*        aString,
                             PRUint32           aLength,
                             nsBoundingMetrics& aBoundingMetrics)
  { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD
  GetBoundingMetricsInternal(const PRUnichar*   aString,
                             PRUint32           aLength,
                             nsBoundingMetrics& aBoundingMetrics,
                             PRInt32*           aFontID = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }
#endif
  NS_IMETHOD DrawStringInternal(const char *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                const nscoord* aSpacing = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD DrawStringInternal(const PRUnichar *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                PRInt32 aFontID = -1,
                                const nscoord* aSpacing = nsnull)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD RenderEPS(const nsRect& aRect, FILE *aDataFile);

#ifdef MOZ_CAIRO_GFX
  NS_IMETHOD Init(nsIDeviceContext* aContext, gfxASurface* aThebesSurface) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Init(nsIDeviceContext* aContext, gfxContext* aThebesContext) { return NS_ERROR_NOT_IMPLEMENTED; }
#endif

protected:
  virtual ~nsRenderingContextImpl();

  /**
   * Determine if a rect's width and height will fit within a specified width and height
   * @param aRect rectangle to test
   * @param aWidth width to determine if the rectangle's width will fit within
   * @param aHeight height to determine if the rectangles height will fit within
   * @returns PR_TRUE if the rect width and height fits with aWidth, aHeight, PR_FALSE
   * otherwise.
   */

  PRBool RectFitsInside(const nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const;

  /**
   * Determine if two rectangles width and height will fit within a specified width and height
   * @param aRect1 first rectangle to test
   * @param aRect1 second rectangle to test
   * @param aWidth width to determine if both rectangle's width will fit within
   * @param aHeight height to determine if both rectangles height will fit within
   * @returns PR_TRUE if the rect1's and rect2's width and height fits with aWidth,
   * aHeight, PR_FALSE otherwise.
   */
  PRBool BothRectsFitInside(const nsRect& aRect1, const nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const;

  /**
   * Return an offscreen surface size from a set of discrete surface sizes.
   * The smallest discrete surface size that can enclose both the Maximum widget 
   * size (@see GetMaxWidgetBounds) and the requested size is returned.
   *
   * @param aMaxBackbufferSize Maximum size that may be requested for the backbuffer
   * @param aRequestedSize Requested size for the offscreen.
   * @param aSurfaceSize contains the surface size 
   */
  void CalculateDiscreteSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSize);

  /**
   * Get the size of the offscreen drawing surface..
   *
   * @param aMaxBackbufferSize Maximum size that may be requested for the backbuffer
   * @param aRequestedSize Desired size for the offscreen.
   * @param aSurfaceSize   Offscreen adjusted to a discrete size which encloses aRequestedSize.
   */
  void GetDrawingSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSurfaceSize);

public:

protected:
  nsPenMode   mPenMode;
private:
  static nsIDrawingSurface*  gBackbuffer;         //singleton backbuffer 
    // Largest requested offscreen size if larger than a full screen.
  static nsSize            gLargestRequestedSize;

};

#endif /* nsRenderingContextImpl */
