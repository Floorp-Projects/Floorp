/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for all rule types in a CSS style sheet */

#include "Rule.h"

#include "mozilla/css/GroupRule.h"
#include "nsCCUncollectableMarker.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace css {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Rule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Rule)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Rule)

bool
Rule::IsCCLeaf() const
{
  return !PreservingWrapper();
}

bool
Rule::IsKnownLive() const
{
  if (HasKnownLiveWrapper()) {
    return true;
  }

  StyleSheet* sheet = GetStyleSheet();
  if (!sheet) {
    return false;
  }

  if (!sheet->IsOwnedByDocument()) {
    return false;
  }

  return nsCCUncollectableMarker::InGeneration(
    sheet->GetAssociatedDocument()->GetMarkedCCGeneration());
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Rule)
  return tmp->IsCCLeaf() || tmp->IsKnownLive();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(Rule)
  // Please see documentation for nsCycleCollectionParticipant::CanSkip* for why
  // we need to check HasNothingToTrace here but not in the other two CanSkip
  // methods.
  return tmp->IsCCLeaf() ||
    (tmp->IsKnownLive() && tmp->HasNothingToTrace(tmp));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Rule)
  return tmp->IsCCLeaf() || tmp->IsKnownLive();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

/* virtual */ void
Rule::SetStyleSheet(StyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = aSheet;
}

NS_IMETHODIMP
Rule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_IF_ADDREF(*aParentRule = mParentRule);
  return NS_OK;
}

NS_IMETHODIMP
Rule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);

  NS_IF_ADDREF(*aSheet = GetStyleSheet());
  return NS_OK;
}

/* virtual */ css::Rule*
Rule::GetCSSRule()
{
  return this;
}

NS_IMETHODIMP
Rule::GetType(uint16_t* aType)
{
  *aType = Type();
  return NS_OK;
}

NS_IMETHODIMP
Rule::SetCssText(const nsAString& aCssText)
{
  // We used to throw for some rule types, but not all.  Specifically, we did
  // not throw for StyleRule.  Let's just always not throw.
  return NS_OK;
}

NS_IMETHODIMP
Rule::GetCssText(nsAString& aCssText)
{
  GetCssTextImpl(aCssText);
  return NS_OK;
}

Rule*
Rule::GetParentRule() const
{
  return mParentRule;
}

} // namespace css
} // namespace mozilla
