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

#ifndef nsIMenuFrame_h___
#define nsIMenuFrame_h___

// {6A4CDE51-6C05-11d3-BB50-00104B7B7DEB}
#define NS_IMENUFRAME_IID \
{ 0x6a4cde51, 0x6c05, 0x11d3, { 0xbb, 0x50, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

class nsIMenuParent;
class nsIDOMElement;

enum nsMenuType {
  eMenuType_Normal = 0,
  eMenuType_Checkbox = 1,
  eMenuType_Radio = 2
};

class nsIMenuFrame : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMENUFRAME_IID; return iid; }

  NS_IMETHOD ActivateMenu(PRBool aFlag) = 0;
  NS_IMETHOD SelectMenu(PRBool aFlag) = 0;
  NS_IMETHOD OpenMenu(PRBool aFlag) = 0;

  NS_IMETHOD MenuIsOpen(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsContainer(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsChecked(PRBool& aResult) = 0;
  NS_IMETHOD MenuIsDisabled(PRBool& aResult) = 0;

  NS_IMETHOD SelectFirstItem() = 0;

  NS_IMETHOD Escape(PRBool& aHandledFlag) = 0;
  NS_IMETHOD Enter() = 0;
  NS_IMETHOD ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag) = 0;
  NS_IMETHOD KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag) = 0;

  NS_IMETHOD GetMenuParent(nsIMenuParent** aMenuParent) = 0;
  NS_IMETHOD GetMenuChild(nsIFrame** aResult) = 0;
 
  NS_IMETHOD GetRadioGroupName(nsString &aName) = 0;
  NS_IMETHOD GetMenuType(nsMenuType &aType) = 0;

  NS_IMETHOD MarkAsGenerated() = 0;

  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult)=0;
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild)=0;
};

#endif

