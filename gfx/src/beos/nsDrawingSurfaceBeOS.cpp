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

#include "nsDrawingSurfaceBeOS.h"
#include "nsCoord.h"

NS_IMPL_ISUPPORTS2(nsDrawingSurfaceBeOS, nsIDrawingSurface, nsIDrawingSurfaceBeOS)

#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
static PRTime mLockTime, mUnlockTime; 
#endif 
 
nsDrawingSurfaceBeOS :: nsDrawingSurfaceBeOS()
{
  NS_INIT_REFCNT();
  mView = NULL;

  mBitmap = nsnull;
  mWidth = 0; 
  mHeight = 0; 
  
  mLockWidth = 0; 
  mLockHeight = 0;
  mLockFlags = 0;
  mLockX = 0; 
  mLockY = 0; 
  mLocked = PR_FALSE;
}

nsDrawingSurfaceBeOS :: ~nsDrawingSurfaceBeOS()
{
  if(mBitmap)
  {
    mBitmap->RemoveChild(mView);
	delete mBitmap;
  }
}

/** 
 * Lock a rect of a drawing surface and return a 
 * pointer to the upper left hand corner of the 
 * bitmap. 
 * @param  aX x position of subrect of bitmap 
 * @param  aY y position of subrect of bitmap 
 * @param  aWidth width of subrect of bitmap 
 * @param  aHeight height of subrect of bitmap 
 * @param  aBits out parameter for upper left hand 
 *         corner of bitmap 
 * @param  aStride out parameter for number of bytes 
 *         to add to aBits to go from scanline to scanline 
 * @param  aWidthBytes out parameter for number of 
 *         bytes per line in aBits to process aWidth pixels 
 * @return error status 
 * 
 **/ 
NS_IMETHODIMP nsDrawingSurfaceBeOS :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
  mLockTime = PR_Now(); 
  //  MOZ_TIMER_RESET(mLockTime); 
  //  MOZ_TIMER_START(mLockTime); 
#endif 

  if (mLocked)
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }
  mLocked = PR_TRUE;

  mLockX = aX; 
  mLockY = aY; 
  mLockWidth = aWidth; 
  mLockHeight = aHeight; 
  mLockFlags = aFlags;

  // Obtain an ximage from the pixmap.  ( I think this copy the bitmap ) 
       // FIX ME !!!!  We need to copy the part locked into the mImage 
  mView->LockLooper(); 

#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
  //  MOZ_TIMER_STOP(mLockTime); 
  //  MOZ_TIMER_LOG(("Time taken to lock: ")); 
  //  MOZ_TIMER_PRINT(mLockTime); 
  printf("Time taken to lock:   %d\n", PR_Now() - mLockTime);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Unlock(void)
{

#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
  mUnlockTime = PR_Now(); 
#endif 

  //  g_print("nsDrawingSurfaceGTK::UnLock() called\n"); 
  if (!mLocked) 
  {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked"); 
    return NS_ERROR_FAILURE;
  }

  // If the lock was not read only, put the bits back on the pixmap
        if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
        {
    // FIX ME!!! 
    // Now draw the image ... 
        }

  // FIX ME!!! 
  // Destroy mImage 
       mView->UnlockLooper();

  mLocked = PR_FALSE; 
 
 
#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
  printf("Time taken to unlock: %d\n", PR_Now() - mUnlockTime);
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
  *aOffScreen = mIsOffscreen;//mBitmap ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: IsPixelAddressable(PRBool *aAddressable)
{
  *aAddressable = PR_FALSE; 
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Init(BView *aView)
{
  if(aView->LockLooper()) 
  { 
    //remember dimensions 
    mWidth=nscoord(aView->Bounds().Width()); 
    mHeight=nscoord(aView->Bounds().Height()); 
    
    mView = aView;

    aView->UnlockLooper(); 
  } 
 
  // XXX was i smoking crack when i wrote this comment? 
  // this is definatly going to be on the screen, as it will be the window of a 
  // widget or something. 
  mIsOffscreen = PR_FALSE; 

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Init(BView *aView, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
  NS_ASSERTION(!(aView == nsnull), "null BView");

  //remember dimensions
  mWidth=aWidth;
  mHeight=aHeight;
  mFlags = aFlags; 
  
  // we can draw on this offscreen because it has no parent 
  mIsOffscreen = PR_TRUE; 

  BRect r(0,0, mWidth-1, mHeight-1);
  mView = new BView(r, "", 0, 0);
  if (!mView)
    return NS_ERROR_OUT_OF_MEMORY;

//if((aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS) &&
//  (aWidth > 0) && (aHeight > 0))
  if(aWidth > 0 && aHeight > 0)
  {
    mBitmap = new BBitmap(r, B_RGBA32, true);
    if (!mBitmap)
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

NS_IMETHODIMP nsDrawingSurfaceBeOS :: AcquireView(BView **aView) 
{
  *aView = mView;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: AcquireBitmap(BBitmap **aBitmap)
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
