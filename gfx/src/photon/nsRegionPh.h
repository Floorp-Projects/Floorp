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

#ifndef nsRegionPh_h___
#define nsRegionPh_h___

#include "nsIRegion.h"
#include "prmem.h"
#include <Pt.h>

class nsRegionPh : public nsIRegion
{
public:
  inline nsRegionPh()
		{
		mRegion = NULL;
		mRegionType = eRegionComplexity_empty;
		}

  inline nsRegionPh(PhTile_t *tiles)
		{
		mRegion = tiles; /* assume ownership */
		mRegionType = (mRegion == NULL) ? eRegionComplexity_empty : eRegionComplexity_complex;
		}

  virtual ~nsRegionPh()
		{
		if( mRegion ) PhFreeTiles( mRegion );
		mRegion = nsnull;
		}

  NS_DECL_ISUPPORTS

  virtual nsresult Init()
		{
		SetRegionEmpty();
		return NS_OK;
		}

  virtual void SetTo(const nsIRegion &aRegion)
		{
		PhTile_t *tiles;
		aRegion.GetNativeRegion( ( void*& ) tiles );
		SetRegionEmpty( );
		mRegion = PhCopyTiles( tiles );
		}

  virtual void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
		{
  	SetRegionEmpty( );

  	if ( aWidth > 0 && aHeight > 0 ) {
  	  /* Create a temporary tile to  assign to mRegion */
  	  PhTile_t *tile = PhGetTile( );
  	  tile->rect.ul.x = aX;
  	  tile->rect.ul.y = aY;
  	  tile->rect.lr.x = (aX+aWidth-1);
  	  tile->rect.lr.y = (aY+aHeight-1);
  	  tile->next = NULL;
  	  mRegion = tile;
  	  }
		}

  virtual void Intersect(const nsIRegion &aRegion)
		{
  	PhTile_t *original = mRegion;
  	PhTile_t *tiles;
  	aRegion.GetNativeRegion( ( void*& ) tiles );
  	mRegion = PhIntersectTilings( original, tiles, NULL);
  	if( mRegion )
  	  mRegion = PhCoalesceTiles( PhMergeTiles( PhSortTiles( mRegion )));
  	PhFreeTiles( original );
  	if ( mRegion == NULL )
  	  SetTo(0, 0, 1, 1);
		}

  virtual void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);

  virtual void Union(const nsIRegion &aRegion)
		{
		PhTile_t *tiles;
		aRegion.GetNativeRegion( ( void*& ) tiles );
		mRegion = PhAddMergeTiles( mRegion, PhCopyTiles( tiles ), NULL );
		}

  virtual void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
		{
		if( aWidth > 0 && aHeight > 0 ) {
		  /* Create a temporary tile to  assign to mRegion */
		  PhTile_t *tile = PhGetTile();
		  tile->rect.ul.x = aX;
		  tile->rect.ul.y = aY;
		  tile->rect.lr.x = (aX+aWidth-1);
		  tile->rect.lr.y = (aY+aHeight-1);
		  tile->next = NULL;

		  mRegion = PhAddMergeTiles( mRegion, tile, NULL );
		  }
		}

  virtual void Subtract(const nsIRegion &aRegion)
		{
		PhTile_t *tiles;
		aRegion.GetNativeRegion((void*&)tiles);
		mRegion = PhClipTilings( mRegion, tiles, NULL );
		}

  virtual void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
		{
		if( aWidth > 0 && aHeight > 0 ) {
		  /* Create a temporary tile to  assign to mRegion */
		  PhTile_t tile;
		  tile.rect.ul.x = aX;
		  tile.rect.ul.y = aY;
		  tile.rect.lr.x = aX + aWidth - 1;
		  tile.rect.lr.y = aY + aHeight - 1;
		  tile.next = NULL;

		  mRegion = PhClipTilings( mRegion, &tile, NULL );
			}
		}

  virtual PRBool IsEmpty(void)
		{
  		if ( !mRegion )
  		  return PR_TRUE;
  		if ( mRegion->rect.ul.x == 0 && mRegion->rect.ul.y == 0 &&
  		  mRegion->rect.lr.x == 0 && mRegion->rect.lr.y == 0 )
  		  return PR_TRUE;
  		return PR_FALSE;
		}

  virtual PRBool IsEqual(const nsIRegion &aRegion);
  virtual void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);

  virtual void Offset(PRInt32 aXOffset, PRInt32 aYOffset)
		{
  	if( ( aXOffset || aYOffset ) && mRegion ) {
  	  PhPoint_t p = { aXOffset, aYOffset };

  	  /* 99.99% there is only one tile - so simplify things, while allow the general case to work as well */
  	  if( !mRegion->next ) PtTranslateRect( &mRegion->rect, &p );
  	  else PhTranslateTiles( mRegion, &p );
  	  }
		}

  virtual PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD GetRects(nsRegionRectSet **aRects);

  inline NS_IMETHODIMP FreeRects(nsRegionRectSet *aRects)
		{
		if( nsnull != aRects ) PR_Free( ( void * )aRects );
		return NS_OK;
		}

  inline NS_IMETHODIMP GetNativeRegion(void *&aRegion) const
		{
		aRegion = (void *) mRegion;
		return NS_OK;
		}

  inline NS_IMETHODIMP GetRegionComplexity(nsRegionComplexity &aComplexity) const
		{
		aComplexity = mRegionType;
		return NS_OK;
		}

  inline NS_IMETHOD GetNumRects(PRUint32 *aRects) const { *aRects = 0; return NS_OK; }

private:
  virtual void SetRegionEmpty()
		{
		if( mRegion ) PhFreeTiles( mRegion );
		mRegion = NULL;
		mRegionType = eRegionComplexity_empty;
		}

  PhTile_t             *mRegion;
  nsRegionComplexity    mRegionType;		// Not really used!
};

#endif  // nsRegionPh_h___ 
