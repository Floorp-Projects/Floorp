/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIRegion_h___
#define nsIRegion_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsRect.h"

enum nsRegionComplexity
{
  eRegionComplexity_empty = 0,
  eRegionComplexity_rect = 1,
  eRegionComplexity_complex = 2
};

typedef struct
{
  PRInt32   x;
  PRInt32   y;
  PRUint32  width;
  PRUint32  height;
} nsRegionRect;

typedef struct
{
  PRUint32      mNumRects;    //number of actual rects in the mRects array
  PRUint32      mRectsLen;    //length, in rects, of the mRects array
  PRUint32      mArea;        //area of the covered portion of the region
  nsRegionRect  mRects[1];
} nsRegionRectSet;

// An implementation of a region primitive that can be used to
// represent arbitrary pixel areas. Probably implemented on top
// of the native region primitive. The assumption is that, at worst,
// it is a rectangle list.

#define NS_IREGION_IID   \
{ 0x8ef366e0, 0xee94, 0x11d1,    \
{ 0xa8, 0x2a, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIRegion : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IREGION_IID)

  virtual nsresult Init(void) = 0;

  /**
  * copy operator equivalent that takes another region
  *
  * @param      region to copy
  * @return     void
  *
  **/

  virtual void SetTo(const nsIRegion &aRegion) = 0;

  /**
  * copy operator equivalent that takes a rect
  *
  * @param      aX xoffset of rect to set region to
  * @param      aY yoffset of rect to set region to
  * @param      aWidth width of rect to set region to
  * @param      aHeight height of rect to set region to
  * @return     void
  *
  **/

  virtual void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;

  /**
  * destructively intersect another region with this one
  *
  * @param      region to intersect
  * @return     void
  *
  **/

  virtual void Intersect(const nsIRegion &aRegion) = 0;

  /**
  * destructively intersect a rect with this region
  *
  * @param      aX xoffset of rect to intersect with region
  * @param      aY yoffset of rect to intersect with region
  * @param      aWidth width of rect to intersect with region
  * @param      aHeight height of rect to intersect with region
  * @return     void
  *
  **/

  virtual void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;

  /**
  * destructively union another region with this one
  *
  * @param      region to union
  * @return     void
  *
  **/

  virtual void Union(const nsIRegion &aRegion) = 0;

  /**
  * destructively union a rect with this region
  *
  * @param      aX xoffset of rect to union with region
  * @param      aY yoffset of rect to union with region
  * @param      aWidth width of rect to union with region
  * @param      aHeight height of rect to union with region
  * @return     void
  *
  **/

  virtual void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;

  /**
  * destructively subtract another region with this one
  *
  * @param      region to subtract
  * @return     void
  *
  **/

  virtual void Subtract(const nsIRegion &aRegion) = 0;

  /**
  * destructively subtract a rect from this region
  *
  * @param      aX xoffset of rect to subtract with region
  * @param      aY yoffset of rect to subtract with region
  * @param      aWidth width of rect to subtract with region
  * @param      aHeight height of rect to subtract with region
  * @return     void
  *
  **/

  virtual void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;
  
  /**
  * is this region empty? i.e. does it contain any pixels
  *
  * @param      none
  * @return     returns whether the region is empty
  *
  **/

  virtual PRBool IsEmpty(void) = 0;

  /**
  * == operator equivalent i.e. do the regions contain exactly
  * the same pixels
  *
  * @param      region to compare
  * @return     whether the regions are identical
  *
  **/

  virtual PRBool IsEqual(const nsIRegion &aRegion) = 0;

  /**
  * returns the bounding box of the region i.e. the smallest
  * rectangle that completely contains the region.        
  *
  * @param      aX out parameter for xoffset of bounding rect for region
  * @param      aY out parameter for yoffset of bounding rect for region
  * @param      aWidth out parameter for width of bounding rect for region
  * @param      aHeight out parameter for height of bounding rect for region
  * @return     void
  *
  **/
  virtual void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight) = 0;

  /**
  * offsets the region in x and y
  *
  * @param  xoffset  pixel offset in x
  * @param  yoffset  pixel offset in y
  * @return          void
  *
  **/
  virtual void Offset(PRInt32 aXOffset, PRInt32 aYOffset) = 0;

  /**
  * does the region intersect the rectangle?
  *
  * @param      rect to check for containment
  * @return     true if the region intersects the rect
  *
  **/

  virtual PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;
  
  /**
   * get the set of rects which make up this region. the aRects
   * parameter must be freed by calling FreeRects before the region
   * is deleted. aRects may be passed in again when requesting
   * the rect list as a recycling method.
   *
   * @param  aRects out parameter containing set of rects
   *                comprising the region
   * @return error status
   *
   **/
  NS_IMETHOD GetRects(nsRegionRectSet **aRects) = 0;

  /**
   * Free a rect set returned by GetRects.
   *
   * @param  aRects rects to free
   * @return error status
   *
   **/
  NS_IMETHOD FreeRects(nsRegionRectSet *aRects) = 0;

  /**
   * Get the native region that this nsIRegion represents.
   * @param aRegion out parameter for native region handle
   * @return error status
   **/
  NS_IMETHOD GetNativeRegion(void *&aRegion) const = 0;

  /**
   * Get the complexity of the region as defined by the
   * nsRegionComplexity enum.
   * @param aComplexity out parameter for region complexity
   * @return error status
   **/
  NS_IMETHOD GetRegionComplexity(nsRegionComplexity &aComplexity) const = 0;

  /**
   * get the number of rects which make up this region.
   *
   * @param  aRects out parameter containing the number of rects
   *                comprising the region
   * @return error status
   *
   **/
  NS_IMETHOD GetNumRects(PRUint32 *aRects) const = 0;
};

#endif  // nsIRegion_h___ 
