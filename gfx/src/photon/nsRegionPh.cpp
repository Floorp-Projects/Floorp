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

/* Local debug flag, this create lots and lots of output */
#undef DEBUG_REGION


static void DumpTiles(PhTile_t *t)
{
#ifdef DEBUG_REGION

  int count=1;
  
  while(t)
  {
    //printf("Tile %d is t=<%p> t->next=<%p> (%d, %d) - (%d,%d)\n", count, t, t->next, tulx, tuly, tlrx, tlry);
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("Tile %d is t=<%p> t->next=<%p> (%d, %d) - (%d,%d)\n", count, t, t->next, tulx, tuly, tlrx, tlry));
    t = t->next;
	count++;
  }
#endif
}

nsRegionPh :: nsRegionPh()
{
  NS_INIT_REFCNT();

  mRegion = NULL;
  mRegionType = eRegionComplexity_empty;
}


nsRegionPh :: ~nsRegionPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::~nsRegion Destructor called\n"));
  
#ifdef DEBUG_REGION
  DumpTiles(mRegion);
#endif

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

  SetRegionEmpty();

  if(( aWidth > 0 ) && ( aHeight > 0 ))
  {
    /* Create a temporary tile to  assign to mRegion */

    PhTile_t *tile = PhGetTile();
    tile->rect.ul.x = aX;
    tile->rect.ul.y = aY;
    tile->rect.lr.x = (aX+aWidth-1);
    tile->rect.lr.y = (aY+aHeight-1);
    tile->next = NULL;
    mRegion = tile;
  }
}


void nsRegionPh :: Intersect(const nsIRegion &aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Intersect with nsIRegion\n"));

  PhTile_t *orig_Tiles = mRegion;
  PhTile_t *tiles;

  aRegion.GetNativeRegion((void*&)tiles);
  mRegion = PhIntersectTilings(orig_Tiles, tiles, NULL);
  if (mRegion)
  {
    mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));  
  }
}


void nsRegionPh :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::Intersect2 aX=<%d> aY=<%d> aWidth=<%d> aHeight=<%d>\n", aX, aY, aWidth, aHeight));

  if(( aWidth > 0 ) && ( aHeight > 0 ))
  {
    /* Create a temporary tile to  assign to mRegion */
    PhTile_t *tile = PhGetTile();
    tile->rect.ul.x = aX;
    tile->rect.ul.y = aY;
    tile->rect.lr.x = (aX+aWidth-1);
    tile->rect.lr.y = (aY+aHeight-1);
    tile->next = NULL;
    
    PhTile_t *orig_Tiles = mRegion;
    mRegion = PhIntersectTilings(mRegion, tile, NULL);
  }
  else
  {
    SetRegionEmpty();
  }
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

  if(( aWidth > 0 ) && ( aHeight > 0 ))
  {
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

  if(( aWidth > 0 ) && ( aHeight > 0 ))
  {
    /* Create a temporary tile to  assign to mRegion */
    PhTile_t *tile = PhGetTile();
    tile->rect.ul.x = aX;
    tile->rect.ul.y = aY;
    tile->rect.lr.x = (aX+aWidth-1);
    tile->rect.lr.y = (aY+aHeight-1);
    tile->next = NULL;

    //printf ("subtract: %d %d %d %d\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y);

    mRegion = PhClipTilings(mRegion, tile, NULL);
  }
}


PRBool nsRegionPh :: IsEmpty(void)
{
  PRBool result = PR_FALSE;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::IsEmpty mRegion=<%p>\n", mRegion));

  if (!mRegion)
    result = PR_TRUE;

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

  if (mRegion)
  {
    mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));  

    /* Create a temporary tile to  assign to mRegion */
    PhTile_t *tile = PhGetTile();
    tile->rect.ul.x = aX;
    tile->rect.ul.y = aY;
    tile->rect.lr.x = (aX+aWidth-1);
    tile->rect.lr.y = (aY+aHeight-1);
    tile->next = NULL;

    if (tile->rect.lr.x == -1)
	  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::ContainsRect problem 5\n"));

    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("testing: %d %d %d %d\n",aX,aY,aWidth,aHeight));
    PhTile_t *test;
    test = PhIntersectTilings(tile, mRegion, NULL);

    if (test)
	  return PR_TRUE;
	else 
	  return PR_FALSE;
  }
  else 
    return PR_FALSE;
}


NS_IMETHODIMP nsRegionPh :: GetRects(nsRegionRectSet **aRects)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetRects mRegion=<%p>\n", mRegion));

  nsRegionRectSet   *rects;
  int               nbox = 0;
  nsRegionRect      *rect;
  PhTile_t	        *t = mRegion;

  /* Count the Tiles */
  t = PhCoalesceTiles( PhMergeTiles( PhSortTiles( t )));  

  while(t)
  {
    nbox++;  
    t = t->next;
  }  

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetRects recty count=<%d>\n", nbox));

  rects = *aRects;

  if ((nsnull == rects) || (rects->mRectsLen < (PRUint32) nbox))
  {
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect) * (nbox - 0)));//was -1
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

  rect->x = 0;
  rect->width = 0;
  rect->y = 0;
  rect->height = 0;

  while (nbox--)
  {
    rect->x = tulx;
    rect->width = (tlrx - tulx+1);
	rect->y = tuly;
    rect->height = (tlry - tuly+1);											  
    rects->mArea += rect->width * rect->height;
    //printf ("getrect: %d %d %d %d\n",rect->x,rect->y,rect->width,rect->height);
    rect++;
    t = t->next;
  }
 
  //printf ("num rects %d %d\n",rects->mNumRects,rects->mRectsLen);  fflush(stdout);
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
  //PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRegionPh::GetNativeRegion mRegion=<%p>\n", mRegion));
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

#ifdef DEBUG_REGION
  DumpTiles(mRegion);
#endif

  if (mRegion)
    PhFreeTiles(mRegion);
	
  mRegion = NULL;
  mRegionType = eRegionComplexity_empty;
}
