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

#define CALL_INNER(inner_, call_)               \
  ((inner_).is<DummyGroupRuleRules>()           \
    ? (inner_).as<DummyGroupRuleRules>().call_  \
    : (inner_).as<ServoGroupRuleRules>().call_)


// -------------------------------
// ServoGroupRuleRules
//

ServoGroupRuleRules::~ServoGroupRuleRules()
{
  if (mRuleList) {
    mRuleList->DropReference();
  }
}

#ifdef DEBUG
void
ServoGroupRuleRules::List(FILE* out, int32_t aIndent) const
{
  // TODO list something reasonable?
}
#endif

size_t
ServoGroupRuleRules::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  // TODO how to implement?
  return 0;
}

// -------------------------------
// GroupRule
//

GroupRule::GroupRule(uint32_t aLineNumber, uint32_t aColumnNumber)
  : Rule(aLineNumber, aColumnNumber)
  , mInner(DummyGroupRuleRules())
{
}

GroupRule::GroupRule(already_AddRefed<ServoCssRules> aRules,
                     uint32_t aLineNumber, uint32_t aColumnNumber)
  : Rule(aLineNumber, aColumnNumber)
  , mInner(ServoGroupRuleRules(Move(aRules)))
{
  mInner.as<ServoGroupRuleRules>().SetParentRule(this);
}

GroupRule::GroupRule(const GroupRule& aCopy)
  : Rule(aCopy)
  , mInner(aCopy.mInner)
{
  CALL_INNER(mInner, SetParentRule(this));
}

GroupRule::~GroupRule()
{
  MOZ_ASSERT(!mSheet, "SetStyleSheet should have been called");
}

NS_IMPL_ADDREF_INHERITED(GroupRule, Rule)
NS_IMPL_RELEASE_INHERITED(GroupRule, Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GroupRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

bool
GroupRule::IsCCLeaf() const
{
  // Let's not worry for now about sorting out whether we're a leaf or not.
  return false;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(GroupRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GroupRule, Rule)
  CALL_INNER(tmp->mInner, SetParentRule(nullptr));
  // If tmp does not have a stylesheet, neither do its descendants.  In that
  // case, don't try to null out their stylesheet, to avoid O(N^2) behavior in
  // depth of group rule nesting.  But if tmp _does_ have a stylesheet (which
  // can happen if it gets unlinked earlier than its owning stylesheet), then we
  // need to null out the stylesheet pointer on descendants now, before we clear
  // tmp->mRules.
  if (tmp->GetStyleSheet()) {
    CALL_INNER(tmp->mInner, SetStyleSheet(nullptr));
  }
  CALL_INNER(tmp->mInner, Clear());
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GroupRule, Rule)
  CALL_INNER(tmp->mInner, Traverse(cb));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ void
GroupRule::SetStyleSheet(StyleSheet* aSheet)
{
  // Don't set the sheet on the kids if it's already the same as the sheet we
  // already have.  This is needed to avoid O(N^2) behavior in group nesting
  // depth when seting the sheet to null during unlink, if we happen to unlin in
  // order from most nested rule up to least nested rule.
  if (aSheet != GetStyleSheet()) {
    CALL_INNER(mInner, SetStyleSheet(aSheet));
    Rule::SetStyleSheet(aSheet);
  }
}


CSSRuleList*
GroupRule::CssRules()
{
  return CALL_INNER(mInner, CssRules(this));
}

uint32_t
GroupRule::InsertRule(const nsAString& aRule, uint32_t aIndex, ErrorResult& aRv)
{
  StyleSheet* sheet = GetStyleSheet();
  if (NS_WARN_IF(!sheet)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  uint32_t count = StyleRuleCount();
  if (aIndex > count) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  NS_ASSERTION(count <= INT32_MAX, "Too many style rules!");

  nsresult rv = sheet->InsertRuleIntoGroup(aRule, this, aIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }
  return aIndex;
}

void
GroupRule::DeleteRule(uint32_t aIndex, ErrorResult& aRv)
{
  StyleSheet* sheet = GetStyleSheet();
  if (NS_WARN_IF(!sheet)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  uint32_t count = StyleRuleCount();
  if (aIndex >= count) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  NS_ASSERTION(count <= INT32_MAX, "Too many style rules!");

  nsresult rv = sheet->DeleteRuleFromGroup(this, aIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

#undef CALL_INNER

} // namespace css
} // namespace mozilla
