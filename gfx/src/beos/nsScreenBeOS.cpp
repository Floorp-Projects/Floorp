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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsScreenBeOS.h"

#include <Screen.h>

nsScreenBeOS :: nsScreenBeOS (  )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenBeOS :: ~nsScreenBeOS()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenBeOS, NS_GET_IID(nsIScreen))


NS_IMETHODIMP
nsScreenBeOS :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
	BScreen screen;
	
  *outTop = 0;
  *outLeft = 0;
  *outWidth = PRInt32(screen.Frame().Width());
  *outHeight = PRInt32(screen.Frame().Height());

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenBeOS :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
	BScreen screen;

  *outTop = 0;
  *outLeft = 0;
  *outWidth = PRInt32(screen.Frame().Width());
  *outHeight = PRInt32(screen.Frame().Height());

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenBeOS :: GetPixelDepth(PRInt32 *aPixelDepth)
{
	BScreen screen;

	color_space depth;
	PRInt32 pixelDepth;
	
	depth = screen.ColorSpace();
	switch(depth)
	{	
		case B_CMAP8:
			pixelDepth = 8;
			break;
		case B_RGB32:
			pixelDepth = 32;
			break;
		case B_RGB15:
			pixelDepth = 15;
			break;
		default:
			printf("FIXME:  Please add this screen depth to the code nsScreenBeOS.cpp\n");
			pixelDepth = 32;
			break;	
	}
  *aPixelDepth = pixelDepth;

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenBeOS :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


