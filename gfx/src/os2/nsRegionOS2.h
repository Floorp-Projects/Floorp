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
 */

// Region object.  From os2fe\regions.cpp.
// Regions are defined in Crazy Region Space - see nsRegionOS2.cpp.

#ifndef _nsRegionOS2_h
#define _nsRegionOS2_h

#include "nsIRegion.h"

typedef void (*nsRECTLInRegionFunc)(void *closure, RECTL &rectl);

class nsRegionOS2 : public nsIRegion
{
 public:
   nsRegionOS2();
   virtual ~nsRegionOS2();
 
   NS_DECL_ISUPPORTS
 
   NS_IMETHOD Init();
 
   virtual void SetTo( const nsIRegion &aRegion);
   virtual void SetTo( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
   virtual void Intersect( const nsIRegion &aRegion);
   virtual void Intersect( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
   virtual void Union( const nsIRegion &aRegion);
   virtual void Union( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
   virtual void Subtract( const nsIRegion &aRegion);
   virtual void Subtract( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
   virtual PRBool IsEmpty();
   virtual PRBool IsEqual( const nsIRegion &aRegion);
   virtual void GetBoundingBox( PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);
   virtual void Offset( PRInt32 aXOffset, PRInt32 aYOffset);
   virtual PRBool ContainsRect( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);

   NS_IMETHOD GetRects( nsRegionRectSet **aRects);
   NS_IMETHOD FreeRects( nsRegionRectSet *aRects);

   // Don't use this -- it returns a region defined in CRS.
   NS_IMETHOD GetNativeRegion( void *&aRegion) const;
   NS_IMETHOD GetRegionComplexity( nsRegionComplexity &aComplexity) const;
 
   // OS/2 specific
   // get region in widget's coord space for given device
   HRGN GetHRGN( PRUint32 ulHeight, HPS hps);

   // copy from another region defined in aWidget's space for a given device
   nsresult Init( HRGN copy, PRUint32 ulHeight, HPS hps);

 private:
   void combine( long lOp, PRInt32 aX, PRInt32 aY, PRInt32 aW, PRInt32 aH);
   void combine( long lOp, const nsIRegion &aRegion);

   HRGN mRegion;
   long mRegionType;
};

#endif
