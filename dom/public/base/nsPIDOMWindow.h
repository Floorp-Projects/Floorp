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


#ifndef nsPIDOMWindow_h__
#define nsPIDOMWindow_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMLocation.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDocument.h"

class nsIDocShell;
class nsIDOMWindowInternal;

#define NS_PIDOMWINDOW_IID \
{ 0x3aa80781, 0x7e6a, 0x11d3, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }


class nsPIDOMWindow : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_PIDOMWINDOW_IID; return iid; }

  NS_IMETHOD GetPrivateParent(nsPIDOMWindow** aResult)=0;
  NS_IMETHOD GetPrivateRoot(nsIDOMWindowInternal** aResult)=0;

  NS_IMETHOD GetLocation(nsIDOMLocation** aLocation) = 0;

  NS_IMETHOD SetObjectProperty(const PRUnichar* aProperty, nsISupports* aObject)=0;
  NS_IMETHOD GetObjectProperty(const PRUnichar* aProperty, nsISupports** aObject)=0;
  
  // This is private because activate/deactivate events are not part of the DOM spec.
  NS_IMETHOD Activate() = 0;
  NS_IMETHOD Deactivate() = 0;

  NS_IMETHOD GetRootCommandDispatcher(nsIDOMXULCommandDispatcher ** aDispatcher)=0;

  /* from nsIBaseWindow */
  /* void setPositionAndSize (in long x, in long y, in long cx, in long cy, in boolean fRepaint); */
  NS_IMETHOD SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint) = 0;

  /* void getPositionAndSize (out long x, out long y, out long cx, out long cy); */
  NS_IMETHOD GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy) = 0;


};

#endif // nsPIDOMWindow_h__
