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
 * Dainis Jonitis, <Dainis_Jonitis@swh-t.lv>.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
  rgnRect.ircStart = 1;
  rgnRect.crc = 0xFFFFFFFF;
  rgnRect.crcReturned = 0;
  rgnRect.ulDirection = RECTDIR_LFRT_TOPBOT;

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
