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

#ifdef NGLAYOUT_DDRAW
#define INITGUID
#endif

#include "nsDrawingSurfacePh.h"
#include "prmem.h"
#include "nsCRT.h"

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
static NS_DEFINE_IID(kIDrawingSurfacePhIID, NS_IDRAWING_SURFACE_PH_IID);


nsDrawingSurfacePh :: nsDrawingSurfacePh()
{
  NS_INIT_REFCNT();

}

nsDrawingSurfacePh :: ~nsDrawingSurfacePh()
{
}

NS_IMETHODIMP nsDrawingSurfacePh :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

  if (aIID.Equals(kIDrawingSurfacePhIID))
  {
    nsIDrawingSurfacePh* tmp = this;
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

NS_IMPL_ADDREF(nsDrawingSurfacePh)
NS_IMPL_RELEASE(nsDrawingSurfacePh)

NS_IMETHODIMP nsDrawingSurfacePh :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Unlock(void)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsOffscreen(PRBool *aOffScreen)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsPixelAddressable(PRBool *aAddressable)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat(nsPixelFormat *aFormat)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t &aGC )
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t &aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetGC( PhGC_t **aGC )
{
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: ReleaseGC( void )
{

  return NS_OK;
}



