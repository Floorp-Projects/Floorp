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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Dean Tessman <dean_tessman@hotmail.com>
 */

#ifndef nsIMenuParent_h___
#define nsIMenuParent_h___


// {D407BF61-3EFA-11d3-97FA-00400553EEF0}
#define NS_IMENUPARENT_IID \
{ 0xd407bf61, 0x3efa, 0x11d3, { 0x97, 0xfa, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIMenuFrame;

class nsIMenuParent : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMENUPARENT_IID; return iid; }

  NS_IMETHOD GetCurrentMenuItem(nsIMenuFrame** aMenuItem) = 0;
  NS_IMETHOD SetCurrentMenuItem(nsIMenuFrame* aMenuItem) = 0;
  NS_IMETHOD GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult) = 0;
  NS_IMETHOD GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult) = 0;

  NS_IMETHOD SetActive(PRBool aActiveFlag) = 0;
  NS_IMETHOD GetIsActive(PRBool& isActive) = 0;
  NS_IMETHOD GetWidget(nsIWidget **aWidget) = 0;
  
  NS_IMETHOD IsMenuBar(PRBool& isMenuBar) = 0;

  NS_IMETHOD DismissChain() = 0;
  NS_IMETHOD HideChain() = 0;
  NS_IMETHOD KillPendingTimers() = 0;

  NS_IMETHOD CreateDismissalListener() = 0;

  NS_IMETHOD InstallKeyboardNavigator() = 0;
  NS_IMETHOD RemoveKeyboardNavigator() = 0;

  // Used to move up, down, left, and right in menus.
  NS_IMETHOD KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag) = 0;
  NS_IMETHOD ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag) = 0;
  // Called when the ESC key is held down to close levels of menus.
  NS_IMETHOD Escape(PRBool& aHandledFlag) = 0;
  // Called to execute a menu item.
  NS_IMETHOD Enter() = 0;

  NS_IMETHOD SetIsContextMenu(PRBool aIsContextMenu) = 0;
  NS_IMETHOD GetIsContextMenu(PRBool& aIsContextMenu) = 0;
  
};

#endif

