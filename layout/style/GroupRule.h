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

#ifndef mozilla_css_GroupRule_h__
#define mozilla_css_GroupRule_h__

#include "mozilla/css/Rule.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"

class nsPresContext;
class nsMediaQueryResultCacheKey;

namespace mozilla {
namespace css {

class GroupRuleRuleList;

// inherits from Rule so it can be shared between
// MediaRule and DocumentRule
class GroupRule : public Rule
{
protected:
  GroupRule();
  GroupRule(const GroupRule& aCopy);
  virtual ~GroupRule();
public:

  // implement part of nsIStyleRule and Rule
  DECL_STYLE_RULE_INHERIT_NO_DOMRULE
  virtual void SetStyleSheet(nsCSSStyleSheet* aSheet);

  // to help implement nsIStyleRule
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

public:
  void AppendStyleRule(Rule* aRule);

  PRInt32 StyleRuleCount() const { return mRules.Count(); }
  Rule* GetStyleRuleAt(PRInt32 aIndex) const;

  typedef nsCOMArray<Rule>::nsCOMArrayEnumFunc RuleEnumFunc;
  bool EnumerateRulesForwards(RuleEnumFunc aFunc, void * aData) const;

  /*
   * The next three methods should never be called unless you have first
   * called WillDirty() on the parent stylesheet.  After they are
   * called, DidDirty() needs to be called on the sheet.
   */
  nsresult DeleteStyleRuleAt(PRUint32 aIndex);
  nsresult InsertStyleRulesAt(PRUint32 aIndex,
                              nsCOMArray<Rule>& aRules);
  nsresult ReplaceStyleRule(Rule *aOld, Rule *aNew);

  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey) = 0;

protected:
  // to help implement nsIDOMCSSRule
  nsresult AppendRulesToCssText(nsAString& aCssText);

  // to implement common methods on nsIDOMCSSMediaRule and
  // nsIDOMCSSMozDocumentRule
  nsresult GetCssRules(nsIDOMCSSRuleList* *aRuleList);
  nsresult InsertRule(const nsAString & aRule, PRUint32 aIndex,
                      PRUint32* _retval);
  nsresult DeleteRule(PRUint32 aIndex);

  nsCOMArray<Rule> mRules;
  nsRefPtr<GroupRuleRuleList> mRuleCollection; // lazily constructed
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_GroupRule_h__ */
