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
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#include "nsScreenOS2.h"
#include "nsGfxDefs.h"

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
NS_IMPL_ISUPPORTS1(nsScreenOS2, nsIScreen)


NS_IMETHODIMP
nsScreenOS2 :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  LONG alArray[2];

  HPS hps = ::WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = GFX (::GpiQueryDevice (hps), HDC_ERROR);

  ::DevQueryCaps(hdc, CAPS_WIDTH, 2, alArray);

  *outTop = 0;
  *outLeft = 0;
  *outWidth = alArray[0];
  *outHeight = alArray[1];

  ::WinReleasePS(hps);

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenOS2 :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outTop = 0;
  *outLeft = 0;
  *outWidth = ::WinQuerySysValue( HWND_DESKTOP, SV_CXFULLSCREEN );
  *outHeight = ::WinQuerySysValue( HWND_DESKTOP, SV_CYFULLSCREEN ); 

  HWND hwndWarpCenter = WinWindowFromID( HWND_DESKTOP, 0x555 );
  if (hwndWarpCenter) {
    SWP swp;
    WinQueryWindowPos( hwndWarpCenter, &swp );
    if (swp.y != 0) {
      /* WarpCenter at top */
      *outTop += swp.cy;
    } /* endif */
  } /* endif */

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenOS2 :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  LONG lCap;

  HPS hps = ::WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = GFX (::GpiQueryDevice (hps), HDC_ERROR);

  GFX (::DevQueryCaps(hdc, CAPS_COLOR_BITCOUNT, 1, &lCap), FALSE);

  *aPixelDepth = (PRInt32)lCap;

  
  ::WinReleasePS(hps);

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenOS2 :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


