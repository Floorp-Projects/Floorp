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
#ifndef nsICSSStyleSheet_h___
#define nsICSSStyleSheet_h___

#include "nsIStyleSheet.h"
#include "nsString.h"

class nsICSSRule;
class nsIDOMNode;
class nsINameSpace;
class nsICSSStyleRuleProcessor;
class nsIMediaList;
class nsICSSGroupRule;

// IID for the nsICSSStyleSheet interface {8f83b0f0-b21a-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_SHEET_IID     \
{0x8f83b0f0, 0xb21a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_STYLE_SHEET_IID; return iid; }

  NS_IMETHOD  ContainsStyleSheet(nsIURI* aURL, PRBool& aContains, nsIStyleSheet** aTheChild=nsnull) = 0;

  NS_IMETHOD  AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;
  NS_IMETHOD  InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex) = 0;

  NS_IMETHOD  PrependStyleRule(nsICSSRule* aRule) = 0;
  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  NS_IMETHOD  DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex) = 0;
  NS_IMETHOD  InsertRuleIntoGroup(nsAReadableString & aRule, nsICSSGroupRule* aGroup, PRUint32 aIndex, PRUint32* _retval) = 0;

  NS_IMETHOD  StyleSheetCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const = 0;

  NS_IMETHOD  Init(nsIURI* aURL) = 0;
  NS_IMETHOD  SetTitle(const nsString& aTitle) = 0;
  NS_IMETHOD  AppendMedium(nsIAtom* aMedium) = 0;
  NS_IMETHOD  ClearMedia(void) = 0;
  NS_IMETHOD  SetOwningNode(nsIDOMNode* aOwningNode) = 0;

  // get head of namespace chain for sheet
  NS_IMETHOD  GetNameSpace(nsINameSpace*& aNameSpace) const = 0;
  // set default namespace for sheet (may be overridden by @namespace)
  NS_IMETHOD  SetDefaultNameSpaceID(PRInt32 aDefaultNameSpaceID) = 0;

  NS_IMETHOD  Clone(nsICSSStyleSheet*& aClone) const = 0;

  NS_IMETHOD  IsModified(PRBool* aModified) const = 0; // returns the mDirty status of the sheet
  NS_IMETHOD  SetModified(PRBool aModified) = 0;

  NS_IMETHOD  DropRuleProcessorReference(nsICSSStyleRuleProcessor* aProcessor) = 0;
};

// XXX for backwards compatibility and convenience
extern NS_EXPORT nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURI* aURL);

extern NS_EXPORT nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);

#endif /* nsICSSStyleSheet_h___ */
