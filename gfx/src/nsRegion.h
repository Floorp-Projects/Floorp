/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsRegion_h___
#define nsRegion_h___

#include "nscore.h"
#include "nsRect.h"

// Function type passed into nsRegion::forEachRect, invoked
// for each rectangle in a region
typedef void (*nsRectInRegionFunc)(void *closure, nsRect& rect);

// An implementation of a region primitive that can be used to
// represent arbitrary pixel areas. Probably implemented on top
// of the native region primitive. The assumption is that, at worst,
// it is a rectangle list.
class nsRegion : public nsObjBase
{
public:
  /**
  * copy operator equivalent that takes another region
  *
  * @param      region to copy
  * @return     void
  *
  **/
  virtual void setRegion(const nsRegion* region) = 0;

  /**
  * copy operator equivalent that takes a rect
  *
  * @param      rect to copy
  * @return     void
  *
  **/
  virtual void setRect(const nsRect* rect) = 0;

  /**
  * destructively intersect another region with this one
  *
  * @param      region to intersect
  * @return     void
  *
  **/
  virtual void intersect(const nsRegion* region) = 0;

  /**
  * destructively intersect a rect with this region
  *
  * @param      rect to intersect
  * @return     void
  *
  **/
  virtual void intersect(const nsRect* rect) = 0;

  /**
  * destructively union another region with this one
  *
  * @param      region to union
  * @return     void
  *
  **/
  virtual void union(const nsRegion* region) = 0;

  /**
  * destructively union a rect with this region
  *
  * @param      rect to union
  * @return     void
  *
  **/
  virtual void union(const nsRect* rect) = 0;

  /**
  * destructively subtract another region with this one
  *
  * @param      region to subtract
  * @return     void
  *
  **/
  virtual void subtract(const nsRegion* region) = 0;
  
  /**
  * is this region empty? i.e. does it contain any pixels
  *
  * @param      none
  * @return     returns whether the region is empty
  *
  **/
  virtual nsbool isEmpty() = 0;

  /**
  * == operator equivalent i.e. do the regions contain exactly
  * the same pixels
  *
  * @param      region to compare
  * @return     whether the regions are identical
  *
  **/
  virtual nsbool isEqual(const nsRegion* region) = 0;

  /**
  * returns the bounding box of the region i.e. the smallest
  * rectangle that completely contains the region.        
  *
  * @param      rect to set to the bounding box
  * @return     void
  *
  **/
  virtual void getBoundingBox(nsRect* rect) = 0;

  /**
  * offsets the region in x and y
  *
  * @param  xoffset  pixel offset in x
  * @param  yoffset  pixel offset in y
  * @return          void
  *
  **/
  virtual void offset(nsfloat xoffset, nsfloat yoffset) = 0;

  /**
  * does the region completely contain the rectangle?
  *
  * @param      rect to check for containment
  * @return     true iff the rect is completely contained
  *
  **/
  virtual nsbool containsRect(const nsRect* rect) = 0;
  
  /**
  * invoke a function for each rectangle in the region
  *
  * @param  func    Function to invoke for each rectangle
  * @param  closure Arbitrary data to pass to the function
  * @return          void
  *
  **/
  virtual void forEachRect(nsRectInRegionFunc* func, void* closure) = 0;
};

extern NS_GFX nsRegion* NS_NewRegion(nsPresentationContext* context);

#endif  // nsRegion_h___ 
