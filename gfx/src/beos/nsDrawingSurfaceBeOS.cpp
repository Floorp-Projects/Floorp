/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDrawingSurfaceBeOS.h"
#include "prmem.h"
#include "nslog.h"

NS_IMPL_LOG(nsDrawingSurfaceBeOSLog)
#define PRINTF NS_LOG_PRINTF(nsDrawingSurfaceBeOSLog)
#define FLUSH  NS_LOG_FLUSH(nsDrawingSurfaceBeOSLog)

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);

nsDrawingSurfaceBeOS :: nsDrawingSurfaceBeOS()
{
  NS_INIT_REFCNT();

  mView = NULL;
  mBitmap = nsnull;
  mWidth = mHeight = 0;
  mLockOffset = mLockHeight = 0;
  mLockFlags = 0;
}

nsDrawingSurfaceBeOS :: ~nsDrawingSurfaceBeOS()
{
  if(mBitmap)
  {
    mBitmap->RemoveChild(mView);
	delete mBitmap;
  }
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMPL_ADDREF(nsDrawingSurfaceBeOS)
NS_IMPL_RELEASE(nsDrawingSurfaceBeOS)

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
#if 0
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
        HBITMAP tbits = ::CreateCompatibleBitmap(mView, 2, 2);
        mLockedBitmap = (HBITMAP)::SelectObject(mView, tbits);

        ::GetObject(mLockedBitmap, sizeof(BITMAP), &mBitmap);

        mLockOffset = aY;
        mLockHeight = min((PRInt32)aHeight, (mBitmap.bmHeight - aY));

        mBitmapInfo = CreateBitmapInfo(mBitmap.bmWidth, mBitmap.bmHeight, mBitmap.bmBitsPixel, (void **)&mDIBits, &mPixFormat);

        if (!(aFlags & NS_LOCK_SURFACE_WRITE_ONLY))
          ::GetDIBits(mView, mLockedBitmap, mLockOffset, mLockHeight, mDIBits, mBitmapInfo, DIB_RGB_COLORS);

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
#endif
	mView->LockLooper();
  PRINTF("nsDrawingSurfaceBeOS :: Lock not implemented\n");

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Unlock(void)
{
	mView->UnlockLooper();
  PRINTF("nsDrawingSurfaceBeOS :: Unlock not implemented\n");
	return NS_OK;
#if 0

#ifdef NGLAYOUT_DDRAW
  NS_ASSERTION(!(mView != nsnull), "attempt to unlock with view");

  if (nsnull == mView)
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
          ::SetDIBits(mView, mLockedBitmap, mLockOffset, mLockHeight, mDIBits, mBitmapInfo, DIB_RGB_COLORS);

        tbits = (HBITMAP)::SelectObject(mView, mLockedBitmap);
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
#endif
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: IsOffscreen(PRBool *aOffScreen)
{
	*aOffScreen = mBitmap ? PR_TRUE : PR_FALSE;
	
	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: IsPixelAddressable(PRBool *aAddressable)
{
	*aAddressable = PR_TRUE;

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: GetPixelFormat(nsPixelFormat *aFormat)
{
	*aFormat = mPixFormat;

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Init(BView *aView)
{
	mView = aView;

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Init(BView *aView, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
	NS_ASSERTION(!(aView == nsnull), "null BView");

       //remember dimensions
       mWidth=aWidth;
       mHeight=aHeight;

	BRect r = aView->Bounds();
	mView = new BView(r, "", 0, 0);
       if (mView==NULL)
               return NS_ERROR_OUT_OF_MEMORY;

//	if((aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS) &&
//		(aWidth > 0) && (aHeight > 0))
	if(aWidth > 0 && aHeight > 0)
	{
		mBitmap = new BBitmap(r, B_RGBA32, true);
               if (mBitmap==NULL)
                       return NS_ERROR_OUT_OF_MEMORY;

               if (mBitmap->InitCheck()!=B_OK) {
                       //for some reason, the bitmap isn't valid - delete the
                       //bitmap object, then indicate failure
                       delete mBitmap;
                       mBitmap=NULL;
                       
                       return NS_ERROR_FAILURE;
	}

               mBitmap->AddChild(mView);
       }

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: GetView(BView **aView)
{
	*aView = mView;

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: GetBitmap(BBitmap **aBitmap)
{
	if(mBitmap && mBitmap->Lock())
	{
		mView->Sync();
		mBitmap->Unlock();
	}
	*aBitmap = mBitmap;

	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: ReleaseView(void)
{
	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: ReleaseBitmap(void)
{
	return NS_OK;
}
