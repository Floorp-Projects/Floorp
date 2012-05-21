/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  NS_MUST_OVERRIDE size_t   // non-virtual -- it is only called by subclasses
    SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const = 0;

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
