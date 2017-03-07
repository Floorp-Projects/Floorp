/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * internal interface representing CSS style rules that contain other
 * rules, such as @media rules
 */

#include "mozilla/css/GroupRule.h"

#include "mozilla/dom/CSSRuleList.h"

using namespace mozilla::dom;

namespace mozilla {
namespace css {

// -------------------------------
// Style Rule List for group rules
//

class GroupRuleRuleList final : public dom::CSSRuleList
{
public:
  explicit GroupRuleRuleList(GroupRule *aGroupRule);

  virtual CSSStyleSheet* GetParentObject() override;

  virtual Rule*
  IndexedGetter(uint32_t aIndex, bool& aFound) override;
  virtual uint32_t
  Length() override;

  void DropReference() { mGroupRule = nullptr; }

private:
  ~GroupRuleRuleList();

private:
  GroupRule* mGroupRule;
};

GroupRuleRuleList::GroupRuleRuleList(GroupRule *aGroupRule)
{
  // Not reference counted to avoid circular references.
  // The rule will tell us when its going away.
  mGroupRule = aGroupRule;
}

GroupRuleRuleList::~GroupRuleRuleList()
{
}

CSSStyleSheet*
GroupRuleRuleList::GetParentObject()
{
  if (!mGroupRule) {
    return nullptr;
  }
  StyleSheet* sheet = mGroupRule->GetStyleSheet();
  return sheet ? sheet->AsGecko() : nullptr;
}

uint32_t
GroupRuleRuleList::Length()
{
  if (!mGroupRule) {
    return 0;
  }

  return AssertedCast<uint32_t>(mGroupRule->StyleRuleCount());
}

Rule*
GroupRuleRuleList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;

  if (mGroupRule) {
    RefPtr<Rule> rule = mGroupRule->GetStyleRuleAt(aIndex);
    if (rule) {
      aFound = true;
      return rule;
    }
  }

  return nullptr;
}

// -------------------------------
// GroupRule
//

GroupRule::GroupRule(uint32_t aLineNumber, uint32_t aColumnNumber)
  : Rule(aLineNumber, aColumnNumber)
{
}

GroupRule::GroupRule(const GroupRule& aCopy)
  : Rule(aCopy)
{
  for (const Rule* rule : aCopy.mRules) {
    RefPtr<Rule> clone = rule->Clone();
    mRules.AppendObject(clone);
    clone->SetParentRule(this);
  }
}

GroupRule::~GroupRule()
{
  MOZ_ASSERT(!mSheet, "SetStyleSheet should have been called");
  for (Rule* rule : mRules) {
    rule->SetParentRule(nullptr);
  }
  if (mRuleCollection) {
    mRuleCollection->DropReference();
  }
}

NS_IMPL_ADDREF_INHERITED(GroupRule, Rule)
NS_IMPL_RELEASE_INHERITED(GroupRule, Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GroupRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

bool
GroupRule::IsCCLeaf() const
{
  // Let's not worry for now about sorting out whether we're a leaf or not.
  return false;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(GroupRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GroupRule, Rule)
  for (Rule* rule : tmp->mRules) {
    rule->SetParentRule(nullptr);
  }
  // If tmp does not have a stylesheet, neither do its descendants.  In that
  // case, don't try to null out their stylesheet, to avoid O(N^2) behavior in
  // depth of group rule nesting.  But if tmp _does_ have a stylesheet (which
  // can happen if it gets unlinked earlier than its owning stylesheet), then we
  // need to null out the stylesheet pointer on descendants now, before we clear
  // tmp->mRules.
  if (tmp->GetStyleSheet()) {
    for (Rule* rule : tmp->mRules) {
      rule->SetStyleSheet(nullptr);
    }
  }
  tmp->mRules.Clear();
  if (tmp->mRuleCollection) {
    tmp->mRuleCollection->DropReference();
    tmp->mRuleCollection = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GroupRule, Rule)
  const nsCOMArray<Rule>& rules = tmp->mRules;
  for (int32_t i = 0, count = rules.Count(); i < count; ++i) {
    if (!rules[i]->IsCCLeaf()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
      cb.NoteXPCOMChild(rules[i]);
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleCollection)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ void
GroupRule::SetStyleSheet(StyleSheet* aSheet)
{
  // Don't set the sheet on the kids if it's already the same as the sheet we
  // already have.  This is needed to avoid O(N^2) behavior in group nesting
  // depth when seting the sheet to null during unlink, if we happen to unlin in
  // order from most nested rule up to least nested rule.
  if (aSheet != GetStyleSheet()) {
    for (Rule* rule : mRules) {
      rule->SetStyleSheet(aSheet);
    }
    Rule::SetStyleSheet(aSheet);
  }
}

#ifdef DEBUG
/* virtual */ void
GroupRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = 0, count = mRules.Count(); index < count; ++index) {
    mRules.ObjectAt(index)->List(out, aIndent + 1);
  }
}
#endif

void
GroupRule::AppendStyleRule(Rule* aRule)
{
  mRules.AppendObject(aRule);
  StyleSheet* sheet = GetStyleSheet();
  aRule->SetStyleSheet(sheet);
  aRule->SetParentRule(this);
  if (sheet) {
    sheet->AsGecko()->SetModifiedByChildRule();
  }
}

Rule*
GroupRule::GetStyleRuleAt(int32_t aIndex) const
{
  return mRules.SafeObjectAt(aIndex);
}

bool
GroupRule::EnumerateRulesForwards(RuleEnumFunc aFunc, void * aData) const
{
  for (const Rule* rule : mRules) {
    if (!aFunc(const_cast<Rule*>(rule), aData)) {
      return false;
    }
  }
  return true;
}

/*
 * The next two methods (DeleteStyleRuleAt and InsertStyleRuleAt)
 * should never be called unless you have first called WillDirty() on
 * the parents stylesheet.  After they are called, DidDirty() needs to
 * be called on the sheet
 */
nsresult
GroupRule::DeleteStyleRuleAt(uint32_t aIndex)
{
  Rule* rule = mRules.SafeObjectAt(aIndex);
  if (rule) {
    rule->SetStyleSheet(nullptr);
    rule->SetParentRule(nullptr);
  }
  return mRules.RemoveObjectAt(aIndex) ? NS_OK : NS_ERROR_ILLEGAL_VALUE;
}

nsresult
GroupRule::InsertStyleRuleAt(uint32_t aIndex, Rule* aRule)
{
  aRule->SetStyleSheet(GetStyleSheet());
  aRule->SetParentRule(this);
  if (! mRules.InsertObjectAt(aRule, aIndex)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
GroupRule::AppendRulesToCssText(nsAString& aCssText) const
{
  aCssText.AppendLiteral(" {\n");

  // get all the rules
  for (int32_t index = 0, count = mRules.Count(); index < count; ++index) {
    Rule* rule = mRules.ObjectAt(index);
    nsAutoString cssText;
    rule->GetCssText(cssText);
    aCssText.AppendLiteral("  ");
    aCssText.Append(cssText);
    aCssText.Append('\n');
  }

  aCssText.Append('}');
}

// nsIDOMCSSMediaRule or nsIDOMCSSMozDocumentRule methods
nsresult
GroupRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  NS_ADDREF(*aRuleList = CssRules());
  return NS_OK;
}

CSSRuleList*
GroupRule::CssRules()
{
  if (!mRuleCollection) {
    mRuleCollection = new css::GroupRuleRuleList(this);
  }

  return mRuleCollection;
}

nsresult
GroupRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  ErrorResult rv;
  *_retval = InsertRule(aRule, aIndex, rv);
  return rv.StealNSResult();
}

uint32_t
GroupRule::InsertRule(const nsAString& aRule, uint32_t aIndex, ErrorResult& aRv)
{
  StyleSheet* sheet = GetStyleSheet();
  if (NS_WARN_IF(!sheet)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  if (aIndex > uint32_t(mRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  NS_ASSERTION(uint32_t(mRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  uint32_t retval;
  nsresult rv =
    sheet->AsGecko()->InsertRuleIntoGroup(aRule, this, aIndex, &retval);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }
  return retval;
}

nsresult
GroupRule::DeleteRule(uint32_t aIndex)
{
  ErrorResult rv;
  DeleteRule(aIndex, rv);
  return rv.StealNSResult();
}

void
GroupRule::DeleteRule(uint32_t aIndex, ErrorResult& aRv)
{
  StyleSheet* sheet = GetStyleSheet();
  if (NS_WARN_IF(!sheet)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (aIndex >= uint32_t(mRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  NS_ASSERTION(uint32_t(mRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  nsresult rv = sheet->AsGecko()->DeleteRuleFromGroup(this, aIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

/* virtual */ size_t
GroupRule::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = mRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mRules.Length(); i++) {
    n += mRules[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mRuleCollection
  return n;
}

// -------------------------------
// ConditionRule
//

ConditionRule::ConditionRule(uint32_t aLineNumber, uint32_t aColumnNumber)
  : GroupRule(aLineNumber, aColumnNumber)
{
}

ConditionRule::ConditionRule(const ConditionRule& aCopy)
  : GroupRule(aCopy)
{
}

ConditionRule::~ConditionRule()
{
}


} // namespace css
} // namespace mozill
