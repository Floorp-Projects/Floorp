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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
class nsIChromeEventHandler;
class nsIFocusController;

#define NS_PIDOMWINDOW_IID \
{ 0x3aa80781, 0x7e6a, 0x11d3, \
 { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsPIDOMWindow : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOW_IID)

  NS_IMETHOD GetPrivateParent(nsPIDOMWindow** aResult) = 0;
  NS_IMETHOD GetPrivateRoot(nsIDOMWindowInternal** aResult) = 0;

  NS_IMETHOD GetObjectProperty(const PRUnichar* aProperty,
                               nsISupports** aObject) = 0;
  
  // This is private because activate/deactivate events are not part
  // of the DOM spec.
  NS_IMETHOD Activate() = 0;
  NS_IMETHOD Deactivate() = 0;

  NS_IMETHOD GetChromeEventHandler(nsIChromeEventHandler** aHandler) = 0;

  NS_IMETHOD HasMutationListeners(PRUint32 aMutationEventType,
                                  PRBool* aResult) = 0;
  NS_IMETHOD SetMutationListeners(PRUint32 aType) = 0;

  NS_IMETHOD GetRootFocusController(nsIFocusController** aResult) = 0;

  NS_IMETHOD ReallyCloseWindow() = 0;
};

#endif // nsPIDOMWindow_h__
