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

#include "nsRegionPh.h"
#include "prmem.h"

#include "nsPhGfxLog.h"


static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

#define tulx t->rect.ul.x
#define tuly t->rect.ul.y
#define tlrx t->rect.lr.x
#define tlry t->rect.lr.y

#define culx c->rect.ul.x
#define culy c->rect.ul.y
#define clrx c->rect.lr.x
#define clry c->rect.lr.y

static void DumpTiles(PhTile_t *t)
{
#if 1
  return;
#else
  int count=1;
  
  while(t)
  {
//  printf("Tile %d is t=<%p> t->next=<%p> (%d, %d) - (%d,%d)\n", count, t, t->next, tulx, tuly, tlrx, tlry);
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("Tile %d is t=<%p> t->next=<%p> (%d, %d) - (%d,%d)\n", count, t, t->next, tulx, tuly, tlrx, tlry));
    t = t->next;
	count++;
  }
#endif
}


static PhTile_t *myIntersectTilings( PhTile_t const * const tile1, PhTile_t const *tile2, unsigned short *num_intersect_tiles )
{
  PhTile_t *dupt1, *intersection = NULL;

  dupt1 = PhCopyTiles( tile1 );
  if (( dupt1 = PhClipTilings( dupt1, tile2, &intersection ) ) != NULL )
  {
     PhFreeTiles( dupt1 );
  }
/*
  if ( num_intersect_tiles )
     for ( dupt1 = intersection,*num_intersect_tiles = 0; dupt1; *num_intersect_tiles++,dupt1 = dupt1->next );
*/

  return(intersection);
}


nsRegionPh :: nsRegionPh()
{
  NS_INIT_REFCNT();
  SetRegionEmpty();
}

nsRegionPh :: ~nsRegionPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::~nsRegion Destructor called\n"));
  
  if (mRegion)
    PhFreeTiles(mRegion);
}

NS_IMPL_QUERY_INTERFACE(nsRegionPh, kRegionIID)
NS_IMPL_ADDREF(nsRegionPh)
NS_IMPL_RELEASE(nsRegionPh)

nsresult nsRegionPh :: Init(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Init\n"));
  return NS_OK;
}

void nsRegionPh :: SetTo(const nsIRegion &aRegion)
{
  PhTile_t *tiles;

  aRegion.GetNativeRegion((void*&) tiles);

  SetRegionEmpty();
  mRegion = PhCopyTiles(tiles);
}

void nsRegionPh :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::SetTo2 aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", aX, aY, aWidth, aHeight));

 /* Create a temporary tile to  assign to mRegion */
  PhTile_t *tile = PhGetTile();
  tile->rect.ul.x = aX;
  tile->rect.ul.y = aY;
  tile->rect.lr.x = (aX+aWidth-1);
  tile->rect.lr.y = (aY+aHeight-1);
  tile->next = NULL;
  
  SetRegionEmpty();
  mRegion = tile;
}

void nsRegionPh :: Intersect(const nsIRegion &aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Intersect with nsIRegion - Not Implemented\n"));

  unsigned short intersected_tiles=0;
  PhTile_t *orig_Tiles = mRegion;
  PhTile_t *tiles;

  aRegion.GetNativeRegion((void*&)tiles);
  mRegion = myIntersectTilings(orig_Tiles, tiles, NULL);
  if (mRegion)
  {
    mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));  
  }
}

void nsRegionPh :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Intersect2 aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", aX, aY, aWidth, aHeight));

 /* Create a temporary tile to  assign to mRegion */
  PhTile_t *tile = PhGetTile();
  tile->rect.ul.x = aX;
  tile->rect.ul.y = aY;
  tile->rect.lr.x = (aX+aWidth-1);
  tile->rect.lr.y = (aY+aHeight-1);
  tile->next = NULL;
    
  unsigned short intersected_tiles;
  PhTile_t *orig_Tiles = mRegion;

  mRegion = myIntersectTilings(mRegion, tile, NULL);

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Intersect with rect intersected_tiles=<%d>\n", intersected_tiles));
 ::DumpTiles(mRegion);
}

void nsRegionPh :: Union(const nsIRegion &aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Union\n"));

  int added;
  PhTile_t *tiles;
  aRegion.GetNativeRegion((void*&)tiles);

  mRegion = PhAddMergeTiles(mRegion, tiles, &added);
}

void nsRegionPh :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Union2 aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", aX, aY, aWidth, aHeight));

 /* Create a temporary tile to  assign to mRegion */
  PhTile_t *tile = PhGetTile();
  tile->rect.ul.x = aX;
  tile->rect.ul.y = aY;
  tile->rect.lr.x = (aX+aWidth-1);
  tile->rect.lr.y = (aY+aHeight-1);
  tile->next = NULL;
    
  int added;

  mRegion = PhAddMergeTiles(mRegion, tile, &added);
}

void nsRegionPh :: Subtract(const nsIRegion &aRegion)
{
  PhTile_t *tiles;
  aRegion.GetNativeRegion((void*&)tiles);

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Subtract with nsIRegion mRegion=<%p> tiles=<%p>\n", mRegion, tiles));

  mRegion = PhClipTilings(mRegion, tiles, NULL);
}

void nsRegionPh :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Subtract aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", aX, aY, aWidth, aHeight));

 /* Create a temporary tile to  assign to mRegion */
  PhTile_t *tile = PhGetTile();
  tile->rect.ul.x = aX;
  tile->rect.ul.y = aY;
  tile->rect.lr.x = (aX+aWidth-1);
  tile->rect.lr.y = (aY+aHeight-1);
  tile->next = NULL;

  mRegion = PhClipTilings(mRegion, tile, NULL);
}

PRBool nsRegionPh :: IsEmpty(void)
{
  PRBool result = PR_TRUE;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::IsEmpty mRegion=<%p>\n", mRegion));

  if (!mRegion)
    return PR_TRUE;

  mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));  
  PhTile_t *t = mRegion;

  while(t)
  {
    /* if width is positive then it is not empty */
    if (tlrx - tulx)
    {
	  result = PR_FALSE;
	  break;
	}
	
    t = t->next;
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::IsEmpty Result=<%d>\n", result));
  return result;
}

PRBool nsRegionPh :: IsEqual(const nsIRegion &aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::IsEqual\n"));
  PRBool result = PR_TRUE;
  PhTile_t *tiles;
  aRegion.GetNativeRegion((void*&)tiles);

  /* If both are equal/NULL then it is equal */
  if (mRegion == tiles)
    return PR_TRUE;
  else if ( (mRegion == NULL) || (tiles == NULL))  /* if either are null */
    return PR_FALSE;
    
  PhSortTiles(mRegion);
  PhSortTiles(tiles);

  PhTile_t *t=mRegion, *c=tiles;
  while(t)    
  {
    if (
	     (tulx != culx) ||  
         (tuly != culy) ||
	     (tlrx != clrx) ||  
         (tlry != clry)		 
       )
    {
      result = PR_FALSE;
      break;	
	}
    t = t->next;  
    c = c->next;
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::IsEqual result=<%d>\n", result));
  return result;
}

void nsRegionPh :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetBoundingBox mRegion=<%p>\n", mRegion));
  int bX=0, bY=0;
  
  *aX = 0;
  *aY = 0;

   PhTile_t *t = mRegion;
   while(t)
   {
     PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetBoundingBox t=<%p> t->next=<%p>\n", t, t->next));

     *aX = min(*aX, tulx);
     *aY = min(*aY, tuly);
     bX = max(bX, tlrx);
	 bY = max(bY, tlry);	 
     t = t->next;   
   }

   *aWidth =  bX - *aX + 1;
   *aHeight = bY - *aY + 1;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetBoundingBox aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", *aX, *aY, *aWidth, *aHeight));
}

void nsRegionPh :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Offset aXOffset=<%d> aYOffset=<%d>\n", aXOffset, aYOffset));
  if (mRegion)
  {
    PhPoint_t p;
	p.x = aXOffset;
	p.y = aYOffset;

    PhTranslateTiles(mRegion, &p);
  }
}

PRBool nsRegionPh :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::ContainsRect mRegion=<%p> (%d,%d) -> (%d,%d)\n", mRegion, aX, aY, aWidth, aHeight));

  PRBool ret = PR_FALSE;

  if (mRegion)
    mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));  

   PhTile_t *t = mRegion;
   while(t)
   {
     if (
	      (tulx <= aX) &&
		  (tuly <= aY) &&
		  (tlrx >= (aX+aWidth-1)) &&
		  (tlry >= (aY+aHeight-1))
		)
	 {
	   ret = PR_TRUE;
	   break;
	 }     

     t = t->next;   
   }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::ContainsRect returning %d\n", ret));
  return ret;
}

NS_IMETHODIMP nsRegionPh :: GetRects(nsRegionRectSet **aRects)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetRects mRegion=<%p>\n", mRegion));

  nsRegionRectSet   *rects;
  int               nbox = 0;
  nsRegionRect      *rect;
  PhTile_t	        *t = mRegion;

  /* Count the Tiles */
  while(t)
  {
    nbox++;  
    t = t->next;
  }  

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetRects recty count=<%d>\n", nbox));


  rects = *aRects;

  if ((nsnull == rects) || (rects->mRectsLen < (PRUint32) nbox))
  {
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect) * (nbox - 1)));
    if (nsnull == buf)
    {
      if (nsnull != rects)
        rects->mNumRects = 0;
      return NS_OK;
    }
		  
    rects = (nsRegionRectSet *) buf;
    rects->mRectsLen = nbox;
  }

  rects->mNumRects = nbox;
  rects->mArea = 0;
  rect = &rects->mRects[0];
  t = mRegion;                  /* Reset tile indexer */
					  
  while (nbox--)
  {
    rect->x = tulx;
    rect->width = (tlrx - tulx+1);
	rect->y = tuly;
    rect->height = (tlry - tuly+1);											  
    rects->mArea += rect->width * rect->height;
    rect++;
    t = t->next;
  }
 
  *aRects = rects;
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: FreeRects(nsRegionRectSet *aRects)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::FreeRects aRects=<%p>\n", aRects));

  if (nsnull != aRects)
    PR_Free((void *)aRects);
  
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: GetNativeRegion(void *&aRegion) const
{
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetNativeRegion mRegion=<%p>\n", mRegion));
  aRegion = (void *) mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetRegionComplexity - Not Implemented\n"));
  aComplexity = mRegionType;
  return NS_OK;
}

void nsRegionPh :: SetRegionEmpty(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::SetRegionEmpty mRegion=<%p>\n", mRegion));

//  if (mRegion)
//    PhFreeTiles(mRegion);
	
  mRegion = NULL;
  mRegionType = eRegionComplexity_empty;
}