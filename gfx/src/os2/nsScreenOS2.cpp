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

#include "nsScreenOS2.h"

#define INCL_PM
#include <os2.h>


nsScreenOS2 :: nsScreenOS2 (  )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenOS2 :: ~nsScreenOS2()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenOS2, NS_GET_IID(nsIScreen))


NS_IMETHODIMP
nsScreenOS2 :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  LONG alArray[2];

  HPS hps = WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = GpiQueryDevice( hps);

  DevQueryCaps(hdc, CAPS_WIDTH, 2, alArray);

  *outTop = 0;
  *outLeft = 0;
  *outWidth = alArray[0];
  *outHeight = alArray[1];

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenOS2 :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outTop = 0;
  *outLeft = 0;
  *outWidth = WinQuerySysValue( HWND_DESKTOP, SV_CXFULLSCREEN );
  *outHeight = WinQuerySysValue( HWND_DESKTOP, SV_CYFULLSCREEN ); 

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenOS2 :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  LONG lCap;

  HPS hps = WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = GpiQueryDevice( hps);

  DevQueryCaps(hdc, CAPS_COLOR_BITCOUNT, 1, &lCap);

  *aPixelDepth = (PRInt32)lCap;

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenOS2 :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


