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

#ifdef NGLAYOUT_DDRAW
#define INITGUID
#endif

#include "nsDrawingSurfaceWin.h"
#include "prmem.h"
#include "nscrt.h"

//#define GFX_DEBUG

#ifdef GFX_DEBUG
  #define BREAK_TO_DEBUGGER           DebugBreak()
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef GFX_DEBUG
  #define VERIFY(exp)                 ((exp) ? 0: (GetLastError(), BREAK_TO_DEBUGGER))
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kIDrawingSurfaceWinIID, NS_IDRAWING_SURFACE_WIN_IID);

#ifdef NGLAYOUT_DDRAW
IDirectDraw *nsDrawingSurfaceWin::mDDraw = NULL;
IDirectDraw2 *nsDrawingSurfaceWin::mDDraw2 = NULL;
nsresult nsDrawingSurfaceWin::mDDrawResult = NS_OK;
#endif

nsDrawingSurfaceWin :: nsDrawingSurfaceWin()
{
  NS_INIT_REFCNT();

  mDC = NULL;
  mOrigBitmap = nsnull;
  mSelectedBitmap = nsnull;
  mKillDC = PR_FALSE;
  mBitmapInfo = nsnull;
  mDIBits = nsnull;
  mLockedBitmap = nsnull;
  mWidth = mHeight = 0;
  mLockOffset = mLockHeight = 0;
  mLockFlags = 0;

#ifdef NGLAYOUT_DDRAW

  CreateDDraw();

  mSurface = NULL;
  mSurfLockCnt = 0;

#endif
}

nsDrawingSurfaceWin :: ~nsDrawingSurfaceWin()
{
  if ((nsnull != mDC) && (nsnull != mOrigBitmap))
  {
    HBITMAP tbits = (HBITMAP)::SelectObject(mDC, mOrigBitmap);

    if (nsnull != tbits)
      VERIFY(::DeleteObject(tbits));

    mOrigBitmap = nsnull;
  }

  if (nsnull != mBitmapInfo)
  {
    PR_Free(mBitmapInfo);
    mBitmapInfo = nsnull;
  }

  if ((nsnull != mDIBits) && (nsnull == mSelectedBitmap))
    PR_Free(mDIBits);

  mDIBits = nsnull;
  mSelectedBitmap = nsnull;

#ifdef NGLAYOUT_DDRAW
  if (NULL != mSurface)
  {
    if (NULL != mDC)
    {
      mSurface->ReleaseDC(mDC);
      mDC = NULL;
    }

    NS_RELEASE(mSurface);
    mSurface = NULL;
  }
  else
#endif
  {
    if (NULL != mDC)
    {
      if (PR_TRUE == mKillDC)
        ::DeleteDC(mDC);

      mDC = NULL;
    }
  }
}

NS_IMETHODIMP nsDrawingSurfaceWin :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDrawingSurfaceIID))
  {
    nsIDrawingSurface* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDrawingSurfaceWinIID))
  {
    nsIDrawingSurfaceWin* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDrawingSurface* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDrawingSurfaceWin)
NS_IMPL_RELEASE(nsDrawingSurfaceWin)

NS_IMETHODIMP nsDrawingSurfaceWin :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
#ifdef NGLAYOUT_DDRAW
  if (mSurfLockCnt == 0)
  {
    RECT  srect;
    DWORD lockflags = 0;

    srect.left = aX;
    srect.top = aY;
    srect.right = aX + aWidth;
    srect.bottom = aY + aHeight;

    if (aFlags & NS_LOCK_SURFACE_READ_ONLY)
      lockflags |= DDLOCK_READONLY;

    if (aFlags & NS_LOCK_SURFACE_WRITE_ONLY)
      lockflags |= DDLOCK_WRITEONLY;

    if (PR_TRUE == LockSurface(mSurface, &mSurfDesc, &mBitmap, &srect, lockflags, &mPixFormat))
      mSurfLockCnt++;
  }
  else
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }

  if (mSurfLockCnt == 0)
#endif
  {
    if (nsnull == mLockedBitmap)
    {
      if (nsnull == mSelectedBitmap)
      {
        HBITMAP tbits = ::CreateCompatibleBitmap(mDC, 2, 2);
        mLockedBitmap = (HBITMAP)::SelectObject(mDC, tbits);

        ::GetObject(mLockedBitmap, sizeof(BITMAP), &mBitmap);

        mLockOffset = aY;
        mLockHeight = min((PRInt32)aHeight, (mBitmap.bmHeight - aY));

        mBitmapInfo = CreateBitmapInfo(mBitmap.bmWidth, mBitmap.bmHeight, mBitmap.bmBitsPixel, (void **)&mDIBits);

        if (!(aFlags & NS_LOCK_SURFACE_WRITE_ONLY))
          ::GetDIBits(mDC, mLockedBitmap, mLockOffset, mLockHeight, mDIBits, mBitmapInfo, DIB_RGB_COLORS);

        mBitmap.bmBits = mDIBits;
      }
      else
      {
        mLockedBitmap = mSelectedBitmap;
        mBitmap.bmBits = mDIBits + mBitmap.bmWidthBytes * aY;
      }
    }
    else
    {
      NS_ASSERTION(0, "nested lock attempt");
      return NS_ERROR_FAILURE;
    }
  }

  *aBits = (PRUint8 *)mBitmap.bmBits + (aX * (mBitmap.bmBitsPixel >> 3));
  *aStride = mBitmap.bmWidthBytes;
  *aWidthBytes = aWidth * (mBitmap.bmBitsPixel >> 3);
  mLockFlags = aFlags;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: Unlock(void)
{
#ifdef NGLAYOUT_DDRAW
  NS_ASSERTION(!(mDC != nsnull), "attempt to unlock with dc");

  if (nsnull == mDC)
  {
    mSurfLockCnt--;

    NS_ASSERTION(!(mSurfLockCnt != 0), "nested surface locks");

    if (mSurfLockCnt == 0)
      mSurface->Unlock(mSurfDesc.lpSurface);
  }
  else
#endif
  {
    if (nsnull != mLockedBitmap)
    {
      if (nsnull == mSelectedBitmap)
      {
        HBITMAP tbits;

        if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
          ::SetDIBits(mDC, mLockedBitmap, mLockOffset, mLockHeight, mDIBits, mBitmapInfo, DIB_RGB_COLORS);

        tbits = (HBITMAP)::SelectObject(mDC, mLockedBitmap);
        ::DeleteObject(tbits);

        if (nsnull != mBitmapInfo)
        {
          PR_Free(mBitmapInfo);
          mBitmapInfo = nsnull;
        }

        if (nsnull != mDIBits)
        {
          PR_Free(mDIBits);
          mDIBits = nsnull;
        }
      }

      mLockedBitmap = nsnull;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mKillDC;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: IsPixelAddressable(PRBool *aAddressable)
{
#ifdef NGLAYOUT_DDRAW
  if (nsnull != mSurface)
    *aAddressable = PR_TRUE;
  else
#endif
  if (nsnull != mSelectedBitmap)
    *aAddressable = PR_TRUE;
  else
    *aAddressable = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: Init(HDC aDC)
{
  mDC = aDC;

  mTechnology = ::GetDeviceCaps(mDC, TECHNOLOGY);

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: Init(HDC aDC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
  NS_ASSERTION(!(aDC == nsnull), "null DC");

#ifdef NGLAYOUT_DDRAW
  if (aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS)
  {
    LPDIRECTDRAWSURFACE ddsurf = nsnull;

    if ((NULL != mDDraw2) && (aWidth > 0) && (aHeight > 0))
    {
      DDSURFACEDESC ddsd;

      ddsd.dwSize = sizeof(ddsd);
      ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
      ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN |
                            ((aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS) ?
                            DDSCAPS_SYSTEMMEMORY : 0);
      ddsd.dwWidth = aWidth;
      ddsd.dwHeight = aHeight;

      mDDraw2->CreateSurface(&ddsd, &ddsurf, NULL);
    }

    if (NULL != ddsurf)
      mSurface = ddsurf;
    else
    {
      mDC = ::CreateCompatibleDC(aDC);
      mKillDC = PR_TRUE;
    }
  }
  else
#endif
  {
    mDC = ::CreateCompatibleDC(aDC);
    mTechnology = ::GetDeviceCaps(mDC, TECHNOLOGY);
    mKillDC = PR_TRUE;
  }

#ifdef NGLAYOUT_DDRAW
  if (nsnull == mSurface)
#endif
  {
    HBITMAP tbits;

    if ((aWidth > 0) && (aHeight > 0))
    {
      if (aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS)
      {
        void        *bits;
        BITMAPINFO  *binfo;
        int         depth;

        depth = ::GetDeviceCaps(aDC, BITSPIXEL);

        binfo = CreateBitmapInfo(aWidth, aHeight, depth);

        if (nsnull != binfo)
          mSelectedBitmap = tbits = ::CreateDIBSection(aDC, binfo, DIB_RGB_COLORS, &bits, NULL, 0);

        if (NULL == mSelectedBitmap)
          tbits = ::CreateCompatibleBitmap(aDC, aWidth, aHeight);
        else
        {
          mBitmapInfo = binfo;
          mDIBits = (PRUint8 *)bits;
          mBitmap.bmWidthBytes = RASWIDTH(aWidth, depth);
          mBitmap.bmBitsPixel = depth;
        }
      }
      else
        tbits = ::CreateCompatibleBitmap(aDC, aWidth, aHeight);
    }
    else
    {
      //we do this to make sure that the memory DC knows what the
      //bitmap format of the original DC was. this way, later
      //operations to create bitmaps from the memory DC will create
      //bitmaps with the correct properties.

      tbits = ::CreateCompatibleBitmap(aDC, 2, 2);
    }

    mOrigBitmap = (HBITMAP)::SelectObject(mDC, tbits);
  }

  mWidth = aWidth;
  mHeight = aHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: GetDC(HDC *aDC)
{
#ifdef NGLAYOUT_DDRAW
  if (nsnull != mSurface)
  {
    if (0 == mSurfLockCnt)
      mSurface->GetDC(&mDC);

    mSurfLockCnt++;
  }
#endif

  *aDC = mDC;

  return NS_OK;
}


NS_IMETHODIMP 
nsDrawingSurfaceWin::GetTECHNOLOGY(PRInt32  *aTechnology)
{ 
  
  *aTechnology = mTechnology;
  return NS_OK;
}



NS_IMETHODIMP nsDrawingSurfaceWin :: ReleaseDC(void)
{
#ifdef NGLAYOUT_DDRAW
  if (nsnull != mSurface)
  {
    --mSurfLockCnt;

    if ((nsnull != mDC) && (mSurfLockCnt == 0))
    {
      mSurface->ReleaseDC(mDC);
      mDC = nsnull;
    }
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceWin :: IsReleaseDCDestructive(PRBool *aDestructive)
{
#ifdef NGLAYOUT_DDRAW
  if (nsnull != mSurface)
    *aDestructive = PR_TRUE;
  else
#endif
    *aDestructive = PR_FALSE;

  return NS_OK;
}

BITMAPINFO * nsDrawingSurfaceWin :: CreateBitmapInfo(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                                                     void **aBits)
{
  PRInt32 palsize, imagesize, spanbytes, allocsize;
  PRUint8 *colortable;
  DWORD   bicomp, masks[3];
  BITMAPINFO  *rv = nsnull;

	switch (aDepth)
  {
		case 8:
			palsize = 256;
			allocsize = 256;
      bicomp = BI_RGB;
      break;

    case 16:
      palsize = 0;
			allocsize = 3;
      bicomp = BI_BITFIELDS;
      masks[0] = 0xf800;
      masks[1] = 0x07e0;
      masks[2] = 0x001f;
     
      mPixFormat.mRedZeroMask = 0x1f;
      mPixFormat.mGreenZeroMask = 0x3f;
      mPixFormat.mBlueZeroMask = 0x1f;
      mPixFormat.mAlphaZeroMask = 0;
      mPixFormat.mRedMask = masks[0];
      mPixFormat.mGreenMask = masks[1];
      mPixFormat.mBlueMask = masks[2];
      mPixFormat.mAlphaMask = 0;
      mPixFormat.mRedCount = 5;
      mPixFormat.mGreenCount = 6;
      mPixFormat.mBlueCount = 5;
      mPixFormat.mAlphaCount = 0;
      mPixFormat.mRedShift = 11;
      mPixFormat.mGreenShift = 5;
      mPixFormat.mBlueShift = 0;
      mPixFormat.mAlphaShift = 0;
  
      break;

		case 24:
      palsize = 0;
			allocsize = 0;
      bicomp = BI_RGB;

     
      mPixFormat.mRedZeroMask = 0xff;
      mPixFormat.mGreenZeroMask = 0xff;
      mPixFormat.mBlueZeroMask = 0xff;
      mPixFormat.mAlphaZeroMask = 0;
      mPixFormat.mRedMask = 0xff;
      mPixFormat.mGreenMask = 0xff00;
      mPixFormat.mBlueMask = 0xff0000;
      mPixFormat.mAlphaMask = 0;
      mPixFormat.mRedCount = 8;
      mPixFormat.mGreenCount = 8;
      mPixFormat.mBlueCount = 8;
      mPixFormat.mAlphaCount = 0;
      mPixFormat.mRedShift = 0;
      mPixFormat.mGreenShift = 8;
      mPixFormat.mBlueShift = 16;
      mPixFormat.mAlphaShift = 0;
     

      break;

		case 32:
      palsize = 0;
			allocsize = 3;
      bicomp = BI_BITFIELDS;
      masks[0] = 0xff0000;
      masks[1] = 0x00ff00;
      masks[2] = 0x0000ff;
     
      mPixFormat.mRedZeroMask = 0xff;
      mPixFormat.mGreenZeroMask = 0xff;
      mPixFormat.mBlueZeroMask = 0xff;
      mPixFormat.mAlphaZeroMask = 0xff;
      mPixFormat.mRedMask = masks[0];
      mPixFormat.mGreenMask = masks[1];
      mPixFormat.mBlueMask = masks[2];
      mPixFormat.mAlphaMask = 0xff000000;
      mPixFormat.mRedCount = 8;
      mPixFormat.mGreenCount = 8;
      mPixFormat.mBlueCount = 8;
      mPixFormat.mAlphaCount = 8;
      mPixFormat.mRedShift = 16;
      mPixFormat.mGreenShift = 8;
      mPixFormat.mBlueShift = 0;
      mPixFormat.mAlphaShift = 24;
   

      break;

		default:
			palsize = -1;
      break;
  }

  if (palsize >= 0)
  {
    spanbytes = RASWIDTH(aWidth, aDepth);
    imagesize = spanbytes * aHeight;

    rv = (BITMAPINFO *)PR_Malloc(sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) * allocsize));

    if (nsnull != rv)
    {
      rv->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	    rv->bmiHeader.biWidth = aWidth;
	    rv->bmiHeader.biHeight = -aHeight;
	    rv->bmiHeader.biPlanes = 1;
	    rv->bmiHeader.biBitCount = (unsigned short)aDepth;
	    rv->bmiHeader.biCompression = bicomp;
	    rv->bmiHeader.biSizeImage = imagesize;
	    rv->bmiHeader.biXPelsPerMeter = 0;
	    rv->bmiHeader.biYPelsPerMeter = 0;
	    rv->bmiHeader.biClrUsed = palsize;
	    rv->bmiHeader.biClrImportant = palsize;

      // set the color table in the info header
	    colortable = (PRUint8 *)rv + sizeof(BITMAPINFOHEADER);

      if ((aDepth == 16) || (aDepth == 32))
        nsCRT::memcpy(colortable, masks, sizeof(DWORD) * allocsize);
      else
	      nsCRT::zero(colortable, sizeof(RGBQUAD) * palsize);

      if (nsnull != aBits)
        *aBits = PR_Malloc(imagesize);
    }
  }

  return rv;
}

#ifdef NGLAYOUT_DDRAW

PRBool nsDrawingSurfaceWin :: LockSurface(IDirectDrawSurface *aSurface, DDSURFACEDESC *aDesc,
                                          BITMAP *aBitmap, RECT *aRect, DWORD aLockFlags, nsPixelFormat *aPixFormat)
{
  if (nsnull != aSurface)
  {
    aDesc->dwSize = sizeof(DDSURFACEDESC);

    if (DD_OK == aSurface->Lock(aRect, aDesc, DDLOCK_WAIT | DDLOCK_SURFACEMEMORYPTR | aLockFlags, NULL))
    {
      if ((aDesc->ddpfPixelFormat.dwFlags &
          (DDPF_ALPHA | DDPF_PALETTEINDEXED1 |
          DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED4 |
          DDPF_PALETTEINDEXEDTO8 | DDPF_YUV | DDPF_ZBUFFER)) ||
          (aDesc->ddpfPixelFormat.dwRGBBitCount < 8))
      {
        //this is a surface that we can't, or don't want to handle.

        aSurface->Unlock(aDesc->lpSurface);
        return PR_FALSE;
      }
      
      aBitmap->bmType = 0;
      aBitmap->bmWidth = aDesc->dwWidth;
      aBitmap->bmHeight = aDesc->dwHeight;
      aBitmap->bmWidthBytes = aDesc->lPitch;
      aBitmap->bmPlanes = 1;
      aBitmap->bmBitsPixel = (PRUint16)aDesc->ddpfPixelFormat.dwRGBBitCount;
      aBitmap->bmBits = aDesc->lpSurface;

      if ((nsnull != aPixFormat) && (aBitmap->bmBitsPixel > 8))
      {
        DWORD btemp, shiftcnt;

        btemp = aDesc->ddpfPixelFormat.dwRBitMask;

        aPixFormat->mRedMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff))
        {
          aPixFormat->mRedShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff))
        {
          aPixFormat->mRedShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else
        {
          aPixFormat->mRedShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mRedShift++;
        }

        aPixFormat->mRedZeroMask = btemp;
        aPixFormat->mRedCount = 0;

        while ((btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mRedCount++;
        }

        btemp = aDesc->ddpfPixelFormat.dwGBitMask;

        aPixFormat->mGreenMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff))
        {
          aPixFormat->mGreenShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff))
        {
          aPixFormat->mGreenShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else
        {
          aPixFormat->mGreenShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mGreenShift++;
        }

        aPixFormat->mGreenZeroMask = btemp;
        aPixFormat->mGreenCount = 0;

        while ((btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mGreenCount++;
        }

        btemp = aDesc->ddpfPixelFormat.dwBBitMask;

        aPixFormat->mBlueMask = btemp;

        shiftcnt = 32;

        if (!(btemp & 0xffff))
        {
          aPixFormat->mBlueShift = 16;
          btemp >>= 16;
          shiftcnt = 16;
        }
        else if (!(btemp & 0xff))
        {
          aPixFormat->mBlueShift = 8;
          btemp >>= 8;
          shiftcnt = 24;
        }
        else
        {
          aPixFormat->mBlueShift = 0;
          shiftcnt = 32;
        }

        while (!(btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mBlueShift++;
        }

        aPixFormat->mBlueZeroMask = btemp;
        aPixFormat->mBlueCount = 0;

        while ((btemp & 1) && shiftcnt--)
        {
          btemp >>= 1;
          aPixFormat->mBlueCount++;
        }

        aPixFormat->mAlphaCount = (PRUint8)aDesc->ddpfPixelFormat.dwAlphaBitDepth;

        if (aPixFormat->mAlphaCount > 0)
        {
          btemp = aDesc->ddpfPixelFormat.dwRGBAlphaBitMask;

          aPixFormat->mAlphaMask = btemp;

          shiftcnt = 32;

          if (!(btemp & 0xffff))
          {
            aPixFormat->mAlphaShift = 16;
            btemp >>= 16;
            shiftcnt = 16;
          }
          else if (!(btemp & 0xff))
          {
            aPixFormat->mAlphaShift = 8;
            btemp >>= 8;
            shiftcnt = 24;
          }
          else
          {
            aPixFormat->mAlphaShift = 0;
            shiftcnt = 32;
          }

          while (!(btemp & 1) && shiftcnt--)
          {
            btemp >>= 1;
            aPixFormat->mAlphaShift++;
          }

          aPixFormat->mAlphaZeroMask = btemp;
        }
        else
        {
          aPixFormat->mAlphaMask = 0;
          aPixFormat->mAlphaShift = 0;
          aPixFormat->mAlphaZeroMask = 0;
        }
      }

      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  else
    return PR_FALSE;
}

nsresult nsDrawingSurfaceWin :: CreateDDraw()
{
  if ((mDDraw2 == NULL) && (mDDrawResult == NS_OK))
  {
    CoInitialize(NULL);

    mDDrawResult = DirectDrawCreate(NULL, &mDDraw, NULL);

    if (mDDrawResult == NS_OK)
      mDDrawResult = mDDraw->QueryInterface(IID_IDirectDraw2, (LPVOID *)&mDDraw2);

    if (mDDrawResult == NS_OK)
    {
      mDDraw2->SetCooperativeLevel(NULL, DDSCL_NORMAL);

#ifdef NS_DEBUG
      printf("using DirectDraw (%08X)\n", mDDraw2);

      DDSCAPS ddscaps;
      DWORD   totalmem, freemem;
    
      ddscaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
      nsresult res = mDDraw2->GetAvailableVidMem(&ddscaps, &totalmem, &freemem);

      if (NS_SUCCEEDED(res))
      {
        printf("total video memory: %d\n", totalmem);
        printf("free video memory: %d\n", freemem);
      }
      else
      {
        printf("GetAvailableVidMem() returned %08x: %s\n", res,
               (res == DDERR_NODIRECTDRAWHW) ?
               "no hardware ddraw driver available" : "unknown error code");
      }
#endif
    }
  }

  return mDDrawResult;
}

nsresult nsDrawingSurfaceWin :: GetDDraw(IDirectDraw2 **aDDraw)
{
  CreateDDraw();

  if (NULL != mDDraw2)
  {
    NS_ADDREF(mDDraw2);
    *aDDraw = mDDraw2;
  }
  else
    *aDDraw = NULL;

  return NS_OK;
}

#endif
