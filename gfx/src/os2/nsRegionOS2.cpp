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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// ToDo: nothing
#include "nsGfxDefs.h"
#include <stdlib.h>
#include <stdio.h>

#include "nsRegionOS2.h"

// Crazy Region Space
//
// In OS/2, windows & presentation spaces have coord. systems with the
// origin in the bottom left & positive going up.
//
// The rest of mozilla assumes a coord. system with the origin in the
// top left & positive going down.
//
// Thus we have a host of methods to convert between the two when
// drawing into a window, and so on.
//
// Regions are different: when they're defined and operations done on
// them, there's no clue to the intended target.  So we need another
// way of defining regions.  Do this using something which is very close
// to XP space (actually much closer now we use nsRects instead of XP_Rects)
// which can be envisaged as a reflection in the (XP space) line y = 0
//
// Hmm, perhaps it would cause less confusion not to mention this at all!

#define nsRgnPS (gModuleData.hpsScreen)

nsRegionOS2::nsRegionOS2()
{
   NS_INIT_REFCNT();

   mRegion = 0;
   mRegionType = RGN_NULL;
}

nsRegionOS2::~nsRegionOS2()
{
   if( mRegion)
   {
      GFX (::GpiDestroyRegion (nsRgnPS, mRegion), FALSE);
   }
}

NS_IMPL_ISUPPORTS(nsRegionOS2, NS_GET_IID(nsIRegion))

// Create empty region
nsresult nsRegionOS2::Init()
{
   mRegion = GFX (::GpiCreateRegion (nsRgnPS, 0, 0), RGN_ERROR);
   mRegionType = RGN_NULL;
   return NS_OK;
}

// assignment
void nsRegionOS2::SetTo( const nsIRegion &aRegion)
{
   nsRegionOS2 *pRegion = (nsRegionOS2 *) &aRegion;

   mRegionType = GFX (::GpiCombineRegion (nsRgnPS, mRegion, pRegion->mRegion,
                                          0, CRGN_COPY), RGN_ERROR);
}

void nsRegionOS2::SetTo( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
   if( 0 == mRegion)
      Init();

   RECTL rcl = { aX, aY, aX + aWidth, aY + aHeight }; // in-ex

   GFX (::GpiSetRegion (nsRgnPS, mRegion, 1, &rcl), FALSE);

   mRegionType = (aWidth && aHeight) ? RGN_RECT : RGN_NULL;
}

// Combine region with something; generic helpers
void nsRegionOS2::combine( long lOp, PRInt32 aX, PRInt32 aY, PRInt32 aW, PRInt32 aH)
{
   RECTL rcl = { aX, aY, aX + aW, aY + aH }; // in-ex
   HRGN rgn = GFX (::GpiCreateRegion (nsRgnPS, 1, &rcl), RGN_ERROR);
   if (rgn == RGN_ERROR)
   {
      printf( "X Y W H is %d %d %d %d\n", aX, aY, aW, aH);
   }
   mRegionType = GFX (::GpiCombineRegion (nsRgnPS, mRegion, mRegion, rgn, lOp), RGN_ERROR);
   GFX (::GpiDestroyRegion (nsRgnPS, rgn), FALSE);
}

void nsRegionOS2::combine( long lOp, const nsIRegion &aRegion)
{
   nsRegionOS2 *pRegion = (nsRegionOS2 *)&aRegion;
   mRegionType = GFX (::GpiCombineRegion (nsRgnPS, mRegion, mRegion,
                                          pRegion->mRegion, lOp), RGN_ERROR);
}

#define DECL_COMBINE(name,token)                         \
void nsRegionOS2::name(const nsIRegion &aRegion)         \
{ combine( token, aRegion); }                            \
                                                         \
void nsRegionOS2::name( PRInt32 aX, PRInt32 aY,          \
                        PRInt32 aWidth, PRInt32 aHeight) \
{ combine( token, aX, aY, aWidth, aHeight); }

DECL_COMBINE(Intersect,CRGN_AND)
DECL_COMBINE(Union,CRGN_OR)
DECL_COMBINE(Subtract,CRGN_DIFF)

// misc
PRBool nsRegionOS2::IsEmpty()
{
  return (mRegionType == RGN_NULL) ? PR_TRUE : PR_FALSE;
}

PRBool nsRegionOS2::IsEqual( const nsIRegion &aRegion)
{
  nsRegionOS2 *pRegion = (nsRegionOS2 *)&aRegion;

  long lrc = GFX (::GpiEqualRegion (nsRgnPS, mRegion, pRegion->mRegion), EQRGN_ERROR);

  return lrc == EQRGN_EQUAL ? PR_TRUE : PR_FALSE;
}

void nsRegionOS2::GetBoundingBox( PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
   if( mRegionType != RGN_NULL)
   {
      RECTL rcl;
      GFX (::GpiQueryRegionBox (nsRgnPS, mRegion, &rcl), RGN_ERROR);
 
      *aX = rcl.xLeft;
      *aY = rcl.yBottom;
      *aWidth = rcl.xRight - rcl.xLeft; // in-ex, okay.
      *aHeight = rcl.yTop - rcl.yBottom;
   }
   else
      *aX = *aY = *aWidth = *aHeight = 0;
}

// translate
void nsRegionOS2::Offset( PRInt32 aXOffset, PRInt32 aYOffset)
{
   POINTL ptl = { aXOffset, aYOffset };
   GFX (::GpiOffsetRegion (nsRgnPS, mRegion, &ptl), FALSE);
}

// hittest - precise spec, rect must be completely contained.
PRBool nsRegionOS2::ContainsRect( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
   RECTL rcl = { aX, aY, aX + aWidth, aY + aHeight }; // in-ex
   long lRC = GFX (::GpiRectInRegion (nsRgnPS, mRegion, &rcl), RRGN_ERROR);
   return lRC == RRGN_INSIDE ? PR_TRUE : PR_FALSE;
}

// Accessor for the CRS region
nsresult nsRegionOS2::GetNativeRegion( void *&aRegion) const
{
   aRegion = (void*) mRegion;
   return NS_OK;
}

// Get the complexity of the region
nsresult nsRegionOS2::GetRegionComplexity( nsRegionComplexity &aComplexity) const
{
   NS_ASSERTION( mRegionType != RGN_ERROR, "Bad region complexity");

   switch( mRegionType)
   {
      case RGN_NULL:    aComplexity = eRegionComplexity_empty;   break;
      case RGN_RECT:    aComplexity = eRegionComplexity_rect;    break;
      default:
      case RGN_COMPLEX: aComplexity = eRegionComplexity_complex; break;
   }

   return NS_OK;
}

// Code recycled from os2fe/drawable.cpp (the good old days...)
// The beautiful thing about this is that it works both ways: os/2
// space in, Crazy Region Space out, and vice-versa.
struct CGetRects
{
   PRECTL pRects;
   ULONG  ulUsed;
   ULONG  ulGot;
   ULONG  ulHeight;

   CGetRects( ULONG h) : ulUsed( 0), ulGot( 10), ulHeight( h)
   {
      pRects = (PRECTL) malloc( 10 * sizeof( RECTL));
   }

  ~CGetRects() { free( pRects); }

   inline void add( RECTL &rectl) // sneaky; might work...
   {
      if( ulUsed == ulGot)
      {
         ulGot += 10;
         pRects = (PRECTL) realloc( pRects, ulGot * sizeof( RECTL));
      }
      pRects[ ulUsed].xLeft = rectl.xLeft;
      pRects[ ulUsed].yBottom = ulHeight - rectl.yTop;  // This is right.
      pRects[ ulUsed].xRight = rectl.xRight;            // Trust me.
      pRects[ ulUsed].yTop = ulHeight - rectl.yBottom;
      ulUsed++;
   }
};

// Big ugly function to accumulate lists of rectangles.
// All logic is in parameters (as opposed to making a callback per rect) for
// speed reasons (maybe spurious, but...)
static void RealQueryRects( HRGN              hrgn,
                            HPS               hps,
                            nsRegionRectSet **aRects,
                            CGetRects        *aGetRects)
{
   BOOL isRECTL = aRects ? FALSE : TRUE;

   // right, this is far too complicated.  What we want is a function to
   // query how many rectangles we need before we start...
   RECTL   rects[ 10];
   RGNRECT rgnRect = { 1, 10, 0, RECTDIR_LFRT_TOPBOT };

   for( ;;)
   {
      // get a batch of rectangles
      GFX (::GpiQueryRegionRects (hps, hrgn, 0, &rgnRect, rects), FALSE);
      // call them out
      for( PRUint32 i = 0; i < rgnRect.crcReturned; i++)
      {
         if( isRECTL)
         {
            aGetRects->add( rects[i]);
         }
         else
         {
            // accumulate nsRects in the nsRegionRectSet structure

            // first check for space
            if( (*aRects)->mNumRects == (*aRects)->mRectsLen)
            {
               *aRects = (nsRegionRectSet *)
                  realloc( *aRects, sizeof( nsRegionRectSet) +
                             ((*aRects)->mNumRects + 9) * sizeof(nsRegionRect));
               (*aRects)->mRectsLen += 10;
#ifdef DEBUG
// !! If this happens lots, bump up initial allocation
               printf( "Allocating more regionrect space...\n");
#endif
            }

            nsRegionRect *theRect = (*aRects)->mRects + (*aRects)->mNumRects;

            theRect->x = rects[i].xLeft;
            theRect->y = rects[i].yBottom;
            theRect->width = rects[i].xRight - rects[i].xLeft;  // in-ex
            theRect->height = rects[i].yTop - rects[i].yBottom;

            (*aRects)->mNumRects++;
         }
      }
      // are we done ?
      if( rgnRect.crcReturned < rgnRect.crc) break;

      // set up for the next batch
      rgnRect.ircStart += 10;
   }
}

#define GetRects_Native(r,p,a) RealQueryRects( r, p, nsnull, a)
#define GetRects_NS(r,p,a) RealQueryRects( r, p, a, nsnull)

HRGN nsRegionOS2::GetHRGN( PRUint32 ulHeight, HPS hps)
{
   CGetRects getRects( ulHeight);

   GetRects_Native( mRegion, nsRgnPS, &getRects);

   return GFX (::GpiCreateRegion (hps, getRects.ulUsed, getRects.pRects), RGN_ERROR);
}

// For copying from an existing region who has height & possibly diff. hdc
nsresult nsRegionOS2::Init( HRGN copy, PRUint32 ulHeight, HPS hps)
{
   CGetRects getRects( ulHeight);

   GetRects_Native( copy, hps, &getRects);

   Init();

   mRegionType = GFX (::GpiSetRegion (nsRgnPS, mRegion, getRects.ulUsed,
                                      getRects.pRects), FALSE);
   return NS_OK;
}

// Get the region as an array of rects for the new compositor
nsresult nsRegionOS2::GetRects( nsRegionRectSet **aRects)
{
   if( !aRects)
      return NS_ERROR_NULL_POINTER;

   if( *aRects == nsnull)
   {
      *aRects = (nsRegionRectSet *) malloc( sizeof( nsRegionRectSet) +
                                            9 * sizeof( nsRegionRect));
      (*aRects)->mNumRects = 0;
      (*aRects)->mRectsLen = 10;
   }
   else
   {
      // Can reuse the structures (says the header).
      // That's quite sensible, actually.
      (*aRects)->mNumRects = 0;
   }

   GetRects_NS( mRegion, nsRgnPS, aRects);

   return NS_OK;
}

nsresult nsRegionOS2::FreeRects( nsRegionRectSet *aRects)
{
   if( !aRects)
      return NS_ERROR_NULL_POINTER;
   free( aRects);
   return NS_OK;
}
