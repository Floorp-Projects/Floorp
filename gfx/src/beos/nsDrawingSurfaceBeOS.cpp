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
 * Contributor(s): Sergei Dolgov
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

#include "nsDrawingSurfaceBeOS.h"
#include "nsCoord.h"

#include <Region.h>

NS_IMPL_ISUPPORTS2(nsDrawingSurfaceBeOS, nsIDrawingSurface, nsIDrawingSurfaceBeOS)

#ifdef CHEAP_PERFORMANCE_MEASUREMENT 
static PRTime mLockTime, mUnlockTime; 
#endif 
 
nsDrawingSurfaceBeOS :: nsDrawingSurfaceBeOS()
{
  mView = nsnull;
  mBitmap = nsnull;
  mWidth = 0; 
  mHeight = 0; 
  mLockFlags = 0;
  mLocked = PR_FALSE;
}

nsDrawingSurfaceBeOS :: ~nsDrawingSurfaceBeOS()
{
  if(mBitmap)
  {
    // Deleting mBitmap will also remove and delete any child views
    mBitmap->Unlock();
    delete mBitmap;
    mView = nsnull;
    mBitmap = nsnull;
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
  mLockFlags = aFlags;

  if (mBitmap && !mLocked)
  {
    if (mView)
      mView->Sync();
    if (mLockFlags & NS_LOCK_SURFACE_READ_ONLY)
      mBitmap->LockBits();
    *aStride = mBitmap->BytesPerRow();
    *aBits = (uint8 *)mBitmap->Bits() + aX*4 + *aStride * aY;
    *aWidthBytes = aWidth*4;
    mLocked = PR_TRUE; 
  }
  else
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceBeOS :: Unlock(void)
{
  if (mBitmap && mLocked)
  {
    if (mLockFlags & NS_LOCK_SURFACE_READ_ONLY)
      mBitmap->UnlockBits();
    mLocked = PR_FALSE; 
  }
  return NS_OK;
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
  if (aView->LockLooper()) 
  { 
    //remember dimensions 
    BRect r = aView->Bounds();
    mWidth = nscoord(r.IntegerWidth() + 1);
    mHeight = nscoord(r.IntegerHeight() + 1);
    
    mView = aView;
    aView->UnlockLooper(); 
  } 
 
  // onscreen View, attached to BWindow, acquired via GetNativeData() call in nsRendering
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
  
  //creating offscreen  backbuffer surface
  mIsOffscreen = PR_TRUE; 
  //TODO: Maybe we should reuse BView by resizing it, 
  //and also reuse BBitmap if new size is = < of current size
  BRect r(0,0, mWidth-1, mHeight-1);
  //creating auxiliary BView to draw on offscreen BBitmap
  mView = new BView(r, "", 0, 0);
  if (!mView)
    return NS_ERROR_OUT_OF_MEMORY;

//if((aFlags & NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS) &&
//  (aWidth > 0) && (aHeight > 0))
  if (aWidth > 0 && aHeight > 0)
  {
    ///creating offscreen BBitmap
    mBitmap = new BBitmap(r, B_RGBA32, true);
    if (!mBitmap)
      return NS_ERROR_OUT_OF_MEMORY;

    if (mBitmap->InitCheck()!=B_OK)
    {
      //for some reason, the bitmap isn't valid - delete the
      //bitmap object, then indicate failure
      delete mBitmap;
      mBitmap=NULL;
      return NS_ERROR_FAILURE;
    }
    
    //NB! Locking bitmap for lifetime to avoid unneccessary locking at each
    //drawing primitive call. Locking is quite time-expensive.
    //To avoid it, we call surface->LockDrawable() instead LockLooper()
    mBitmap->Lock();
    //Setting ViewColor transparent noticeably decreases AppServer load in DrawBitmp()
    //Applicable here, because Mozilla paints backgrounds explicitly, with images or filling areas.
    mView->SetViewColor(B_TRANSPARENT_32_BIT);
    mBitmap->AddChild(mView);
    // Import prototype onscreen view state
    if (aView && aView->LockLooper())
    {
      BRegion region;
      BFont font;
      mView->SetHighColor(aView->HighColor());
      mView->SetLowColor(aView->LowColor());
      aView->GetFont(&font);
      mView->SetFont(&font);
      aView->GetClippingRegion(&region);
      mView->ConstrainClippingRegion(&region);
      mView->SetOrigin(aView->Origin());
      mView->SetFlags(aView->Flags());
      aView->UnlockLooper();
    } 
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
  if (mBitmap && mView)
  {
    mView->Sync();
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

bool nsDrawingSurfaceBeOS :: LockDrawable()
{
  //TODO: try to avoid exta locking also for onscreen BView.
  //Perhaps it needs synchronization with widget through nsToolkit and lock counting.
  bool rv = false;
  if (!mBitmap)
  {
    // Non-bitmap (BWindowed) view - lock it as required if exists
    rv = mView && mView->LockLooper();
  }
  else
  {
    // Was locked in Init(), only test for locked state here
  	rv = mBitmap->IsLocked();
  }
  return rv;
}

void nsDrawingSurfaceBeOS :: UnlockDrawable()
{
  // Do nothing, bitmap is locked for lifetime in our implementation
  if (mBitmap)
    return;
  // Non-bitmap (BWindowed) view - unlock it as required.
  // mBitmap may be gone in destroy process, so additional check for Looper()
  if (mView && mView->Looper())
    mView->UnlockLooper();
}
