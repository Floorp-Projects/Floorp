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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#ifndef nsICSSGroupRule_h___
#define nsICSSGroupRule_h___

#include "nsICSSRule.h"
#include "nsISupportsArray.h"

class nsIAtom;

// IID for the nsICSSGroupRule interface {5af048aa-1af0-11d3-9d83-0060088f9ff7}
#define NS_ICSS_GROUP_RULE_IID     \
{0x5af048aa, 0x1af0, 0x11d3, {0x9d, 0x83, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

class nsICSSGroupRule : public nsICSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_GROUP_RULE_IID; return iid; }

  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  NS_IMETHOD  EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const = 0;

  /*
   * The next two methods (DeleteStyleRuleAt and InsertStyleRulesAt)
   * should never be called unless you have first called WillDirty()
   * on the parent stylesheet.  After they are called, DidDirty()
   * needs to be called on the sheet
   */
  NS_IMETHOD  DeleteStyleRuleAt(PRUint32 aIndex) = 0;
  NS_IMETHOD  InsertStyleRulesAt(PRUint32 aIndex, nsISupportsArray* aRules) = 0;

   
};

#endif /* nsICSSGroupRule_h___ */
