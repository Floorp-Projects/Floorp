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
 *		John C. Griggs <johng@corel.com>
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

#include "nsScreenManagerQT.h"
#include "nsScreenQT.h"

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gSMCount = 0;
PRUint32 gSMID = 0;
#endif

nsScreenManagerQT::nsScreenManagerQT()
{
#ifdef DBG_JCG
  gSMCount++;
  mID = gSMID++;
  printf("JCG: nsScreenManagerQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gSMCount);
#endif
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerQT::~nsScreenManagerQT()
{
#ifdef DBG_JCG
  gSMCount--;
  printf("JCG: nsScreenManagerQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gSMCount);
#endif
  // nothing to see here.
}

// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerQT, nsIScreenManager)

//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
// NOTE: For this "single-monitor" impl, we just always return the cached primary
//        screen. This should change when a multi-monitor impl is done.
//
nsIScreen* 
nsScreenManagerQT::CreateNewScreenObject()
{
  nsIScreen *retval = nsnull;
  if (!mCachedMainScreen)
    mCachedMainScreen = new nsScreenQT();
  NS_IF_ADDREF(retval = mCachedMainScreen.get());
  
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
nsScreenManagerQT::ScreenForRect(PRInt32 /*inLeft*/, PRInt32 /*inTop*/, 
				 PRInt32 /*inWidth*/, PRInt32 /*inHeight*/, 
				 nsIScreen **outScreen)
{
  GetPrimaryScreen(outScreen);
  return NS_OK;
} // ScreenForRect

//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerQT::GetPrimaryScreen(nsIScreen **aPrimaryScreen) 
{
  *aPrimaryScreen = CreateNewScreenObject();    // addrefs  
  return NS_OK;
} // GetPrimaryScreen

//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerQT::GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
  *aNumberOfScreens = 1;
  return NS_OK;
} // GetNumberOfScreens
