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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMScreen.h"
#include "nsISupports.h"
#include "nscore.h"
#include "nsIScriptContext.h"

class nsIDocShell;
class nsIDeviceContext;

// Script "screen" object
class ScreenImpl : public nsIScriptObjectOwner, public nsIDOMScreen {
public:
  ScreenImpl( nsIDocShell* aDocShell );
  virtual ~ScreenImpl();

  NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  NS_IMETHOD GetTop(PRInt32* aWidth);
  NS_IMETHOD GetLeft(PRInt32* aWidth);
  NS_IMETHOD GetWidth(PRInt32* aWidth);
  NS_IMETHOD GetHeight(PRInt32* aHeight);
  NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
  NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);
  NS_IMETHOD GetAvailWidth(PRInt32* aAvailWidth);
  NS_IMETHOD GetAvailHeight(PRInt32* aAvailHeight);
  NS_IMETHOD GetAvailLeft(PRInt32* aAvailLeft);
  NS_IMETHOD GetAvailTop(PRInt32* aAvailTop);


protected:
	nsIDeviceContext* GetDeviceContext();
	
  void *mScriptObject;
  nsIDocShell* mDocShell; // Weak Reference
};

#endif /* nsScreen_h___ */
