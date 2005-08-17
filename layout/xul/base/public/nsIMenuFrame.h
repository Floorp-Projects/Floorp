/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIMenuFrame_h___
#define nsIMenuFrame_h___

// {2281EFC8-A8BA-4a73-8CF7-DB4EECA5EAEC}
#define NS_IMENUFRAME_IID \
{ 0x2281efc8, 0xa8ba, 0x4a73, { 0x8c, 0xf7, 0xdb, 0x4e, 0xec, 0xa5, 0xea, 0xec } };

class nsIMenuParent;
class nsIDOMElement;
class nsIDOMKeyEvent;

enum nsMenuType {
  eMenuType_Normal = 0,
  eMenuType_Checkbox = 1,
  eMenuType_Radio = 2
};

class nsIMenuFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMENUFRAME_IID)

  NS_IMETHOD ActivateMenu(PRBool aActivate) = 0;
  NS_IMETHOD SelectMenu(PRBool aFlag) = 0;
  NS_IMETHOD OpenMenu(PRBool aFlag) = 0;

  NS_IMETHOD MenuIsOpen(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsContainer(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsChecked(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsDisabled(PRBool& aResult) = 0;

  NS_IMETHOD SelectFirstItem() = 0;

  NS_IMETHOD Escape(PRBool& aHandledFlag) = 0;
  NS_IMETHOD Enter() = 0;
  NS_IMETHOD ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag) = 0;
  NS_IMETHOD KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag) = 0;

  virtual nsIMenuParent *GetMenuParent() = 0;
  virtual nsIFrame *GetMenuChild() = 0;
 
  NS_IMETHOD GetRadioGroupName(nsString &aName) = 0;
  NS_IMETHOD GetMenuType(nsMenuType &aType) = 0;

  NS_IMETHOD MarkAsGenerated() = 0;

  NS_IMETHOD UngenerateMenu() = 0;

  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult)=0;
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild)=0;
};

#endif

