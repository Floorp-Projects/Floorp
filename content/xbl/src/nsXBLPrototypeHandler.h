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

#ifndef nsXBLPrototypeHandler_h__
#define nsXBLPrototypeHandler_h__

#include "nsIXBLPrototypeHandler.h"
#include "nsIAtom.h"
#include "nsString.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;

#define NS_HANDLER_TYPE_XBL_JS      0
#define NS_HANDLER_TYPE_XBL_COMMAND 1
#define NS_HANDLER_TYPE_XUL         2

#define NS_PHASE_BUBBLING           0
#define NS_PHASE_TARGET             1
#define NS_PHASE_CAPTURING          2

class nsXBLPrototypeHandler : public nsIXBLPrototypeHandler
{
public:
  // This constructor is used by XBL handlers (both the JS and command shorthand variety)
  nsXBLPrototypeHandler(nsAReadableString* aEvent, nsAReadableString* aPhase,
                        nsAReadableString* aAction, nsAReadableString* aCommand,
                        nsAReadableString* aKeyCode, nsAReadableString* aCharCode,
                        nsAReadableString* aModifiers, nsAReadableString* aButton,
                        nsAReadableString* aClickCount); 

  // This constructor is used only by XUL key handlers (e.g., <key>)
  nsXBLPrototypeHandler(nsIContent* aKeyElement);

  virtual ~nsXBLPrototypeHandler();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD MouseEventMatched(nsIAtom* aEventType, nsIDOMMouseEvent* aEvent, PRBool* aResult);
  NS_IMETHOD KeyEventMatched(nsIAtom* aEventType, nsIDOMKeyEvent* aEvent, PRBool* aResult);

  NS_IMETHOD GetHandlerElement(nsIContent** aResult);

  NS_IMETHOD SetHandlerText(const nsAReadableString& aText);

  NS_IMETHOD GetPhase(PRUint8* aResult) { *aResult = mPhase; return NS_OK; };

  NS_IMETHOD GetNextHandler(nsIXBLPrototypeHandler** aResult);
  NS_IMETHOD SetNextHandler(nsIXBLPrototypeHandler* aHandler);

  NS_IMETHOD ExecuteHandler(nsIDOMEventReceiver* aReceiver, nsIDOMEvent* aEvent);

  NS_IMETHOD GetEventName(nsIAtom** aResult);
  NS_IMETHOD SetEventName(nsIAtom* aName) { mEventName = aName; return NS_OK; };

  NS_IMETHOD BindingAttached(nsIDOMEventReceiver* aReceiver);
  NS_IMETHOD BindingDetached(nsIDOMEventReceiver* aReceiver);
  
public:
  static nsresult GetTextData(nsIContent *aParent, nsString& aResult);

  static PRUint32 gRefCnt;
  
protected:
  NS_IMETHOD GetController(nsIDOMEventReceiver* aReceiver, nsIController** aResult);
  
  inline PRInt32 GetMatchingKeyCode(const nsAReadableString& aKeyName);
  void ConstructPrototype(nsIContent* aKeyElement, 
                          nsAReadableString* aEvent=nsnull, nsAReadableString* aPhase=nsnull,
                          nsAReadableString* aAction=nsnull, nsAReadableString* aCommand=nsnull,
                          nsAReadableString* aKeyCode=nsnull, nsAReadableString* aCharCode=nsnull,
                          nsAReadableString* aModifiers=nsnull, nsAReadableString* aButton=nsnull,
                          nsAReadableString* aClickCount=nsnull);

  void GetEventType(nsAWritableString& type);
  PRBool ModifiersMatchMask(nsIDOMUIEvent* aEvent);

  inline PRBool KeyEventMatched(nsIDOMKeyEvent* aKeyEvent);
  inline PRBool MouseEventMatched(nsIDOMMouseEvent* aMouseEvent);

  PRInt32 KeyToMask(PRInt32 key);
  
  static PRInt32 kAccelKey;
  static PRInt32 kMenuAccessKey;
  static void InitAccessKeys();

  static const PRInt32 cShift;
  static const PRInt32 cAlt;
  static const PRInt32 cControl;
  static const PRInt32 cMeta;

protected:
  union {
    nsIContent* mHandlerElement;  // For XUL <key> element handlers.
    PRUnichar* mHandlerText;      // For XBL handlers (we don't build an element for the <handler>,
                                  // and instead we cache the JS text or command name that we should
                                  // use.
  };
  
  // The following four values make up 32 bits.
  PRUint8 mPhase;            // The phase (capturing, bubbling)
  PRUint8 mKeyMask;          // Which modifier keys this event handler expects to have down
                             // in order to be matched.
  PRUint8 mType;             // The type of the handler.  The handler is either a XUL key
                             // handler, an XBL "command" event, or a normal XBL event with
                             // accompanying JavaScript.
  PRUint8 mMisc;             // Miscellaneous extra information.  For key events,
                             // stores whether or not we're a key code or char code.
                             // For mouse events, stores the clickCount.

  // The primary filter information for mouse/key events.
  PRInt32 mDetail;           // For key events, contains a charcode or keycode. For
                             // mouse events, stores the button info.
  
  

  nsCOMPtr<nsIXBLPrototypeHandler> mNextHandler; // Prototype handlers are chained. We own the next handler in the chain.
  nsCOMPtr<nsIAtom> mEventName; // The type of the event, e.g., "keypress"
};

extern nsresult
NS_NewXBLPrototypeHandler(nsAReadableString* aEvent, nsAReadableString* aPhase,
                          nsAReadableString* aAction, nsAReadableString* aCommand,
                          nsAReadableString* aKeyCode, nsAReadableString* aCharCode,
                          nsAReadableString* aModifiers, nsAReadableString* aButton,
                          nsAReadableString* aClickCount, nsIXBLPrototypeHandler** aResult);

extern nsresult
NS_NewXULKeyHandler(nsIContent* aHandlerElement, nsIXBLPrototypeHandler** aResult);

#endif
