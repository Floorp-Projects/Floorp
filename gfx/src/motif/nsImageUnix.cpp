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
#include "nsImageUnix.h"
#include "nsRenderingContextUnix.h"
#include "nsDeviceContextUnix.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageUnix :: nsImageUnix()
{
  NS_INIT_REFCNT();
  
  mImage = nsnull ;
}

//------------------------------------------------------------

nsImageUnix :: ~nsImageUnix()
{
  if (nsnull != mImage) {
    XDestroyImage(mImage);
    mImage = nsnull;
  }
}

NS_IMPL_ISUPPORTS(nsImageUnix, kIImageIID);

//------------------------------------------------------------

nsresult nsImageUnix :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mMaskReq = aMaskRequirements;

  // create the memory for the image
  ComputeMetrics();
  mImageBits = new unsigned char[mSizeImage];

  return NS_OK;
}

//------------------------------------------------------------

void nsImageUnix::ComputMetrics()
{

  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;

}

//------------------------------------------------------------

PRInt32  nsImageWin :: CalcBytesSpan(PRUint32  aWidth)
{
PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

	if (((PRUint32)aWidth * mDepth) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return(spanbytes);
}

//------------------------------------------------------------

// set up the pallete to the passed in color array, RGB only in this array
void nsImageUnix :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  if (nsnull == mImage) {
    CreateImage(aContext);
  }

  if (nsnull == mImage)
    return;


  if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, aFlags)){

  }

}


//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                          PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  if (nsnull == mImage)
    return PR_FALSE;

  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                          PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (nsnull == mImage)
    return PR_FALSE;

  return PR_TRUE;
}

//------------------------------------------------------------

void nsImageUnix::CompositeImage(nsIImage *aTheImage, nsPoint *aULLocation,nsBlendQuality aBlendQuality)
{

}

//------------------------------------------------------------

// lets build an alpha mask from this image
PRBool nsImageUnix::SetAlphaMask(nsIImage *aTheMask)
{
  return(PR_FALSE);
}

//------------------------------------------------------------

nsresult nsImageUnix::Optimize(nsDrawingSurface aDrawingSurface)
{
  return(NS_OK);
}

//------------------------------------------------------------

void nsImageUnix::CreateImage(nsIDeviceContext * aDeviceContext)
{

  /* The XImage must be compatible with the Pixmap we draw into */
  nsDrawingSurfaceUnix * surface = (nsDrawingSurfaceUnix *) 
    ((nsDeviceContextUnix *)aDeviceContext)->GetDrawingSurface();

  XWindowAttributes wa;
  PRUint32 wdepth;
  Visual * visual ;
  PRUint32 format ;

  char * data = (char *) PR_Malloc(sizeof(char) * mWidth * mHeight * mDepth);

  ::XGetWindowAttributes(surface->display,
			 surface->drawable,
			 &wa);
  
  /* XXX What if mDepth != wDepth */
  wdepth = wa.depth;
  visual = wa.visual;

  /* Need to support monochrome too */
  if (visual->c_class == TrueColor || visual->c_class == DirectColor)
    format = ZPixmap;
  else
    format = XYPixmap;

  mImage = ::XCreateImage(surface->display,
			  visual,
			  wdepth,
			  format,
			  0,
			  data,
			  mWidth, 
			  mHeight,
			  8,0);

  return ;
}
