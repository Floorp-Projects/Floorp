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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/*
 * internal interface representing CSS style rules that contain other
 * rules, such as @media rules
 */

#ifndef nsICSSGroupRule_h___
#define nsICSSGroupRule_h___

#include "nsICSSRule.h"
#include "nsCOMArray.h"

class nsIAtom;
class nsPresContext;
class nsMediaQueryResultCacheKey;

// IID for the nsICSSGroupRule interface {67b8492e-6d8d-43a5-8037-71eb269f24fe}
#define NS_ICSS_GROUP_RULE_IID     \
{0x67b8492e, 0x6d8d, 0x43a5, {0x80, 0x37, 0x71, 0xeb, 0x26, 0x9f, 0x24, 0xfe}}

class nsICSSGroupRule : public nsICSSRule {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_GROUP_RULE_IID)

  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  typedef nsCOMArray<nsICSSRule>::nsCOMArrayEnumFunc RuleEnumFunc;
  NS_IMETHOD_(PRBool) EnumerateRulesForwards(RuleEnumFunc aFunc, void * aData) const = 0;

  /*
   * The next three methods should never be called unless you have first
   * called WillDirty() on the parent stylesheet.  After they are
   * called, DidDirty() needs to be called on the sheet.
   */
  NS_IMETHOD  DeleteStyleRuleAt(PRUint32 aIndex) = 0;
  NS_IMETHOD  InsertStyleRulesAt(PRUint32 aIndex,
                                 nsCOMArray<nsICSSRule>& aRules) = 0;
  NS_IMETHOD  ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew) = 0;

  NS_IMETHOD_(PRBool) UseForPresentation(nsPresContext* aPresContext,
                                         nsMediaQueryResultCacheKey& aKey) = 0;
   
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSGroupRule, NS_ICSS_GROUP_RULE_IID)

#endif /* nsICSSGroupRule_h___ */
