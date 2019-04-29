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

GroupRule::GroupRule(already_AddRefed<ServoCssRules> aRules, StyleSheet* aSheet,
                     Rule* aParentRule, uint32_t aLineNumber,
                     uint32_t aColumnNumber)
    : Rule(aSheet, aParentRule, aLineNumber, aColumnNumber),
      mRuleList(new ServoCSSRuleList(std::move(aRules), aSheet, this)) {}

GroupRule::~GroupRule() {
  MOZ_ASSERT(!mSheet, "SetStyleSheet should have been called");
  if (mRuleList) {
    mRuleList->DropReferences();
  }
}

NS_IMPL_ADDREF_INHERITED(GroupRule, Rule)
NS_IMPL_RELEASE_INHERITED(GroupRule, Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GroupRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

bool GroupRule::IsCCLeaf() const {
  // Let's not worry for now about sorting out whether we're a leaf or not.
  return false;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(GroupRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GroupRule, Rule)
  if (tmp->mRuleList) {
    // If tmp has a style sheet (which can happen if it gets unlinked
    // earlier than its owning style sheet), then we need to null out the
    // style sheet pointer on descendants now, before we clear mRuleList.
    tmp->mRuleList->DropReferences();
    tmp->mRuleList = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GroupRule, Rule)
  ImplCycleCollectionTraverse(cb, tmp->mRuleList, "mRuleList");
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#ifdef DEBUG
void GroupRule::List(FILE* out, int32_t aIndent) const {
  // TODO list something reasonable?
}
#endif

/* virtual */
void GroupRule::DropSheetReference() {
  if (mRuleList) {
    mRuleList->DropSheetReference();
  }
  Rule::DropSheetReference();
}

uint32_t GroupRule::InsertRule(const nsAString& aRule, uint32_t aIndex,
                               ErrorResult& aRv) {
  if (IsReadOnly()) {
    return 0;
  }

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

void GroupRule::DeleteRule(uint32_t aIndex, ErrorResult& aRv) {
  if (IsReadOnly()) {
    return;
  }

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

size_t GroupRule::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  // TODO how to implement?
  return 0;
}

}  // namespace css
}  // namespace mozilla
