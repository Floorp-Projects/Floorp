/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Dainis Jonitis,
 * <Dainis_Jonitis@swh-t.lv>.  Portions created by Dainis Jonitis are
 * Copyright (C) 2001 Dainis Jonitis. All Rights Reserved.
 * 
 * Contributor(s): 
 */

#include "nsGfxDefs.h"
#include "nsRegionOS2.h"


NS_IMPL_ISUPPORTS1(nsRegionOS2, nsIRegion)

nsRegionOS2::nsRegionOS2() 
{  
}

// Do not use GetNativeRegion on OS/2. Regions are device specific. Use GetHRGN () instead.
nsresult nsRegionOS2::GetNativeRegion (void *&aRegion) const
{
  aRegion = (void*)RGN_ERROR;
  return NS_OK;
}

PRUint32 nsRegionOS2::NumOfRects (HPS aPS, HRGN aRegion) const
{
  RGNRECT rgnRect;
  rgnRect.crcReturned = 0;

  GFX (::GpiQueryRegionRects (aPS, aRegion, NULL, &rgnRect, NULL), FALSE);
 
  return rgnRect.crcReturned;
}

HRGN nsRegionOS2::GetHRGN (PRUint32 DestHeight, HPS DestPS)
{
  PRUint32 NumRects = mRegion.GetNumRects ();

  if (NumRects > 0)
  {
    PRECTL pRects = new RECTL [NumRects];

    nsRegionRectIterator ri (mRegion);
    const nsRect* pSrc;
    PRECTL pDest = pRects;

    while (pSrc = ri.Next ())
    {
      pDest->xLeft    = pSrc->x;
      pDest->xRight   = pSrc->XMost ();
      pDest->yTop     = DestHeight - pSrc->y;
      pDest->yBottom  = pDest->yTop - pSrc->height;
      pDest++;
    }

    HRGN rgn = GFX (::GpiCreateRegion (DestPS, NumRects, pRects), RGN_ERROR);
    delete [] pRects;

    return rgn;
  } else
  {
    return GFX (::GpiCreateRegion (DestPS, 0, NULL), RGN_ERROR);
  }
}

// For copying from an existing region who has height & possibly diff. hdc
nsresult nsRegionOS2::InitWithHRGN (HRGN SrcRegion, PRUint32 SrcHeight, HPS SrcPS)
{
  PRUint32 NumRects = NumOfRects (SrcPS, SrcRegion);
  mRegion.SetEmpty ();

  if (NumRects > 0)
  {
    RGNRECT RgnControl = { 1, NumRects, 0, RECTDIR_LFRT_TOPBOT };
    PRECTL  pRects = new RECTL [NumRects];

    GFX (::GpiQueryRegionRects (SrcPS, SrcRegion, NULL, &RgnControl, pRects), FALSE);

    for (PRUint32 cnt = 0 ; cnt < NumRects ; cnt++)
      mRegion.Or (mRegion, nsRect ( pRects [cnt].xLeft, SrcHeight - pRects [cnt].yTop, 
                  pRects [cnt].xRight - pRects [cnt].xLeft, pRects [cnt].yTop - pRects [cnt].yBottom));

    delete [] pRects;
  }

  return NS_OK;
}
