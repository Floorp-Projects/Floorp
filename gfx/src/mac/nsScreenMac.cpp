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

#include "nsScreenMac.h"
#include <Menus.h>


nsScreenMac :: nsScreenMac ( GDHandle inScreen )
  : mScreen(inScreen)
{
  NS_INIT_REFCNT();

  NS_ASSERTION ( inScreen, "Passing null device to nsScreenMac" );
  
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenMac :: ~nsScreenMac()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenMac, NS_GET_IID(nsIScreen))


NS_IMETHODIMP 
nsScreenMac :: GetWidth(PRInt32 *aWidth)
{
  *aWidth = (**mScreen).gdRect.right - (**mScreen).gdRect.left;
  return NS_OK;

} // GetWidth


NS_IMETHODIMP 
nsScreenMac :: GetHeight(PRInt32 *aHeight)
{
  *aHeight = (**mScreen).gdRect.bottom - (**mScreen).gdRect.top;
  return NS_OK;

} // GetHeight


NS_IMETHODIMP 
nsScreenMac :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  *aPixelDepth = (**(**mScreen).gdPMap).pixelSize;
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenMac :: GetColorDepth(PRInt32 *aColorDepth)
{
  *aColorDepth = (**(**mScreen).gdPMap).pixelSize;
  return NS_OK;

} // GetColorDepth


NS_IMETHODIMP 
nsScreenMac :: GetAvailWidth(PRInt32 *aAvailWidth)
{
  GetWidth(aAvailWidth);
  return NS_OK;

} // GetAvailWidth


NS_IMETHODIMP 
nsScreenMac :: GetAvailHeight(PRInt32 *aAvailHeight)
{
  Rect adjustedRect;
  SubtractMenuBar ( (**mScreen).gdRect, &adjustedRect );
  *aAvailHeight = adjustedRect.bottom - adjustedRect.top;
  
  return NS_OK;

} // GetAvailHeight


NS_IMETHODIMP 
nsScreenMac :: GetAvailLeft(PRInt32 *aAvailLeft)
{
  *aAvailLeft = (**mScreen).gdRect.left;
  return NS_OK;

} // GetAvailLeft


NS_IMETHODIMP 
nsScreenMac :: GetAvailTop(PRInt32 *aAvailTop)
{
  Rect adjustedRect;
  SubtractMenuBar ( (**mScreen).gdRect, &adjustedRect );
  *aAvailTop = adjustedRect.top;
  
  return NS_OK;

} // GetAvailTop


//
// SubtractMenuBar
//
// Take out the menu bar from the reported size of this screen, but only
// if it is the primary screen
//
void
nsScreenMac :: SubtractMenuBar ( const Rect & inScreenRect, Rect* outAdjustedRect )
{
  // if the screen we're being asked about is the main screen (ie, has the menu
  // bar), then subract out the menubar from the rect that we're returning.
  *outAdjustedRect = inScreenRect;  
  if ( IsPrimaryScreen() )
    outAdjustedRect->top += ::GetMBarHeight();
}

