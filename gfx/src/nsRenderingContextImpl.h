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


typedef struct {	
    double x;	  // x coordinate of edge's intersection with current scanline */
    double dx;	// change in x with respect to y 
    int i;	    // edge number: edge i goes from mPointList[i] to mPointList[i+1] 
} Edge;

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

class nsRenderingContextImpl : public nsIRenderingContext
{

// CLASS MEMBERS
public:


protected:
  nsTransform2D		  *mTranMatrix;				// The rendering contexts transformation matrix
  nsLineStyle       mLineStyle;					// The current linestyle, currenly used on mac, other platfroms to follow
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

  /**
   * Sets the Pen Mode for the RenderingContext 
   * @param aPenMode The Pen Mode
   * @return NS_OK if the Pen Mode is correctly set
   */
  NS_IMETHOD SetPenMode(nsPenMode aPenMode) { return NS_ERROR_FAILURE;};

  NS_IMETHOD GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer); 
  NS_IMETHOD ReleaseBackbuffer(void);
  NS_IMETHOD DestroyCachedBackbuffer(void);
  NS_IMETHOD UseBackbuffer(PRBool* aUseBackbuffer);
  
  /**
   * Let the device context know whether we want text reordered with
   * right-to-left base direction
   */
  NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);

  NS_IMETHOD DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect);
  NS_IMETHOD DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset, const nsRect * aTargetRect);

  NS_IMETHOD RenderPostScriptDataFragment(const unsigned char *aData, unsigned long aDatalen);

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

  /**
   * Utility method used to implement NS_IMETHOD GetBackbuffer
   *
   * @param aRequestedSize size of the backbuffer area requested
   * @param aMaxSize maximum size that may be requested for the backbuffer
   * @param aBackbuffer drawing surface used as the backbuffer
   * @param aCacheBackbuffer PR_TRUE then the backbuffer will be cached, if PR_FALSE it is created each time
   */
  nsresult AllocateBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer, PRBool aCacheBackbuffer);

public:

protected:
  nsPenMode   mPenMode;
private:
  static nsDrawingSurface  gBackbuffer;         //singleton backbuffer 
  static nsRect            gBackbufferBounds;   //backbuffer bounds
    // Largest requested offscreen size if larger than a full screen.
  static nsSize            gLargestRequestedSize;

};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

#endif /* nsRenderingContextImpl */
