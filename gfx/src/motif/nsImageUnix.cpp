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

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageUnix :: nsImageUnix()
{
  NS_INIT_REFCNT();
}

//------------------------------------------------------------

nsImageUnix :: ~nsImageUnix()
{
}

NS_IMPL_ISUPPORTS(nsImageUnix, kIImageIID);

//------------------------------------------------------------

nsresult nsImageUnix :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  return NS_OK;
}

//------------------------------------------------------------

// set up the pallete to the passed in color array, RGB only in this array
void nsImageUnix :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
}


//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                          PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  return PR_TRUE;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
PRBool nsImageUnix :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                          PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
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

nsresult nsImageUnix::Optimize(nsDrawingSurface aDrawingSurface)
{
  return(NS_OK);
}

nsIImage * nsImageUnix::DuplicateImage()
{
  return(nsnull);
}
