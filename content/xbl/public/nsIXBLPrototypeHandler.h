/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

/*

  Private interface to the XBL PrototypeHandler

*/

#ifndef nsIXBLPrototypeHandler_h__
#define nsIXBLPrototypeHandler_h__

#include "nsAReadableString.h"

class nsIContent;
class nsIDOMEvent;
class nsIDOMMouseEvent;
class nsIDOMKeyEvent;
class nsIController;
class nsIAtom;
class nsIDOMEventReceiver;

// {921812E7-A044-4bd8-B49E-69BB0A607202}
#define NS_IXBLPROTOTYPEHANDLER_IID \
{ 0x921812e7, 0xa044, 0x4bd8, { 0xb4, 0x9e, 0x69, 0xbb, 0xa, 0x60, 0x72, 0x2 } }

class nsIXBLPrototypeHandler : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLPROTOTYPEHANDLER_IID; return iid; }

  NS_IMETHOD MouseEventMatched(nsIAtom* aEventType, nsIDOMMouseEvent* aEvent, PRBool* aResult) = 0;
  NS_IMETHOD KeyEventMatched(nsIAtom* aEventType, nsIDOMKeyEvent* aEvent, PRBool* aResult) = 0;

  NS_IMETHOD GetHandlerElement(nsIContent** aResult) = 0;
  NS_IMETHOD SetHandlerText(const nsAReadableString& aText) = 0;

  NS_IMETHOD GetPhase(PRUint8* aPhase) = 0;

  NS_IMETHOD BindingAttached(nsIDOMEventReceiver* aRec)=0;
  NS_IMETHOD BindingDetached(nsIDOMEventReceiver* aRec)=0;

  NS_IMETHOD GetNextHandler(nsIXBLPrototypeHandler** aResult) = 0;
  NS_IMETHOD SetNextHandler(nsIXBLPrototypeHandler* aHandler) = 0;

  NS_IMETHOD ExecuteHandler(nsIDOMEventReceiver* aReceiver, nsIDOMEvent* aEvent) = 0;

  NS_IMETHOD GetEventName(nsIAtom** aResult) = 0;
  NS_IMETHOD SetEventName(nsIAtom* aName) = 0;
};

extern nsresult
NS_NewXBLPrototypeHandler(nsAReadableString* aEvent, nsAReadableString* aPhase,
                          nsAReadableString* aAction, nsAReadableString* aCommand,
                          nsAReadableString* aKeyCode, nsAReadableString* aCharCode,
                          nsAReadableString* aModifiers, nsAReadableString* aButton,
                          nsAReadableString* aClickCount,
                          nsIXBLPrototypeHandler** aResult);

extern nsresult
NS_NewXULKeyHandler(nsIContent* aHandlerElement, nsIXBLPrototypeHandler** aResult);

#endif // nsIXBLPrototypeHandler_h__
