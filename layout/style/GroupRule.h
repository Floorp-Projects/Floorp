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
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/css/Rule.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {

class ErrorResult;
class StyleSheet;

namespace dom {
class CSSRuleList;
}  // namespace dom

namespace css {

// Inherits from Rule so it can be shared between MediaRule and DocumentRule
class GroupRule : public Rule {
 protected:
  GroupRule(StyleSheet* aSheet, Rule* aParentRule, uint32_t aLineNumber,
            uint32_t aColumnNumber);
  virtual ~GroupRule();
  virtual already_AddRefed<StyleLockedCssRules> GetOrCreateRawRules() = 0;

 public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GroupRule, Rule)
  NS_DECL_ISUPPORTS_INHERITED

  GroupRule(const GroupRule&) = delete;
  bool IsCCLeaf() const override;

  bool IsGroupRule() const final { return true; }

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  void DropSheetReference() override;
  uint32_t StyleRuleCount() { return CssRules()->Length(); }
  Rule* GetStyleRuleAt(int32_t aIndex) { return CssRules()->GetRule(aIndex); }

  void DidSetRawAfterClone() {
    if (mRuleList) {
      mRuleList->SetRawAfterClone(GetOrCreateRawRules());
    }
  }

  /*
   * The next method should never be called unless you have first called
   * WillDirty() on the parent stylesheet.
   */
  nsresult DeleteStyleRuleAt(uint32_t aIndex) {
    return CssRules()->DeleteRule(aIndex);
  }

  // non-virtual -- it is only called by subclasses
  size_t SizeOfExcludingThis(MallocSizeOf) const;
  size_t SizeOfIncludingThis(MallocSizeOf) const override = 0;

  // WebIDL API
  ServoCSSRuleList* CssRules();
  uint32_t InsertRule(const nsACString& aRule, uint32_t aIndex,
                      ErrorResult& aRv);
  void DeleteRule(uint32_t aIndex, ErrorResult& aRv);

 private:
  RefPtr<ServoCSSRuleList> mRuleList;
};

// Implementation of WebIDL CSSConditionRule.
class ConditionRule : public GroupRule {
 protected:
  using GroupRule::GroupRule;

 public:
  virtual void GetConditionText(nsACString& aConditionText) = 0;
};

}  // namespace css
}  // namespace mozilla

#endif /* mozilla_css_GroupRule_h__ */
