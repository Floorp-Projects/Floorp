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

#include "nsScreenManagerWin.h"
#include "nsScreenWin.h"


nsScreenManagerWin :: nsScreenManagerWin ( )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerWin :: ~nsScreenManagerWin()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenManagerWin, NS_GET_IID(nsIScreenManager))


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
nsIScreen* 
nsScreenManagerWin :: CreateNewScreenObject (  )
{
  nsIScreen* retval = new nsScreenWin ( );
  NS_IF_ADDREF(retval);
  return retval;
}


//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerWin :: ScreenForRect ( PRInt32 inTop, PRInt32 inLeft, PRInt32 inWidth, PRInt32 inHeight,
                                        nsIScreen **outScreen )
{
#if 0
  if ( !(inTop || inLeft || inWidth || inHeight) ) {
    NS_WARNING ( "trying to find screen for sizeless window" );
    *outScreen = CreateNewScreenObject ( ::GetMainDevice() );    // addrefs
    return NS_OK;
  }

  Rect globalWindowBounds = { inTop, inLeft, inTop + inHeight, inLeft + inWidth };

  GDHandle currDevice = ::GetDeviceList();
  GDHandle deviceWindowIsOn = ::GetMainDevice();
  PRInt32 greatestArea = 0;
  while ( currDevice ) {
    if ( ::TestDeviceAttribute(currDevice, screenDevice) && ::TestDeviceAttribute(currDevice, screenActive) ) {
      // calc the intersection.
      Rect intersection;
      Rect devRect = (**currDevice).gdRect;
      ::SectRect ( &globalWindowBounds, &devRect, &intersection );
      PRInt32 intersectArea = (intersection.right - intersection.left) * 
                                  (intersection.bottom - intersection.top);
      if ( intersectArea > greatestArea ) {
        greatestArea = intersectArea;
        deviceWindowIsOn = currDevice;
     }      
    } // if device is a screen and visible
    currDevice = ::GetNextDevice(currDevice);
  } // foreach device in list

  *outScreen = CreateNewScreenObject ( deviceWindowIsOn );    // addrefs
#endif
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerWin :: GetPrimaryScreen(nsIScreen * *aPrimaryScreen) 
{
//  *aPrimaryScreen = CreateNewScreenObject ( ::GetMainDevice() );    // addrefs  
  return NS_OK;
  
} // GetPrimaryScreen

