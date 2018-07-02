/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * internal interface representing CSS style rules that contain other
 * rules, such as @media rules
 */

#ifndef mozilla_css_GroupRule_h__
#define mozilla_css_GroupRule_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/css/Rule.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {

class StyleSheet;

namespace dom {
class CSSRuleList;
} // namespace dom

namespace css {

// inherits from Rule so it can be shared between
// MediaRule and DocumentRule
class GroupRule : public Rule
{
protected:
  GroupRule(already_AddRefed<ServoCssRules> aRules,
            StyleSheet* aSheet,
            Rule* aParentRule,
            uint32_t aLineNumber,
            uint32_t aColumnNumber);
  GroupRule(const GroupRule& aCopy) = delete;
  virtual ~GroupRule();
public:

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GroupRule, Rule)
  NS_DECL_ISUPPORTS_INHERITED
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  void DropSheetReference() override;

public:

  int32_t StyleRuleCount() const { return mRuleList->Length(); }

  Rule* GetStyleRuleAt(int32_t aIndex) const {
    return mRuleList->GetRule(aIndex);
  }

  /*
   * The next method should never be called unless you have first called
   * WillDirty() on the parent stylesheet.
   */
  nsresult DeleteStyleRuleAt(uint32_t aIndex) {
    return mRuleList->DeleteRule(aIndex);
  }

  // non-virtual -- it is only called by subclasses
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override = 0;

  // WebIDL API
  dom::CSSRuleList* CssRules() { return mRuleList; }
  uint32_t InsertRule(const nsAString& aRule, uint32_t aIndex,
                      ErrorResult& aRv);
  void DeleteRule(uint32_t aIndex, ErrorResult& aRv);

private:
  RefPtr<ServoCSSRuleList> mRuleList;
};

// Implementation of WebIDL CSSConditionRule.
class ConditionRule : public GroupRule
{
protected:
  using GroupRule::GroupRule;

public:
  virtual void GetConditionText(nsAString& aConditionText) = 0;
  virtual void SetConditionText(const nsAString& aConditionText,
                                ErrorResult& aRv) = 0;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_GroupRule_h__ */
