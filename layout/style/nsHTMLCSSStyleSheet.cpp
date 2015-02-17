/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#include "nsHTMLCSSStyleSheet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/StyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "nsPresContext.h"
#include "nsRuleWalker.h"
#include "nsRuleProcessorData.h"
#include "mozilla/dom/Element.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "RestyleManager.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

PLDHashOperator
ClearAttrCache(const nsAString& aKey, MiscContainer*& aValue, void*)
{
  // Ideally we'd just call MiscContainer::Evict, but we can't do that since
  // we're iterating the hashtable.
  MOZ_ASSERT(aValue->mType == nsAttrValue::eCSSStyleRule);

  aValue->mValue.mCSSStyleRule->SetHTMLCSSStyleSheet(nullptr);
  aValue->mValue.mCached = 0;

  return PL_DHASH_REMOVE;
}

} // anonymous namespace

nsHTMLCSSStyleSheet::nsHTMLCSSStyleSheet()
{
}

nsHTMLCSSStyleSheet::~nsHTMLCSSStyleSheet()
{
  // We may go away before all of our cached style attributes do,
  // so clean up any that are left.
  mCachedStyleAttrs.Enumerate(ClearAttrCache, nullptr);
}

NS_IMPL_ISUPPORTS(nsHTMLCSSStyleSheet, nsIStyleRuleProcessor)

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  ElementRulesMatching(aData->mPresContext, aData->mElement,
                       aData->mRuleWalker);
}

void
nsHTMLCSSStyleSheet::ElementRulesMatching(nsPresContext* aPresContext,
                                          Element* aElement,
                                          nsRuleWalker* aRuleWalker)
{
  // just get the one and only style rule from the content's STYLE attribute
  css::StyleRule* rule = aElement->GetInlineStyleRule();
  if (rule) {
    rule->RuleMatched();
    aRuleWalker->Forward(rule);
  }

  rule = aElement->GetSMILOverrideStyleRule();
  if (rule) {
    RestyleManager* restyleManager = aPresContext->RestyleManager();
    if (!restyleManager->SkipAnimationRules()) {
      // Animation restyle (or non-restyle traversal of rules)
      // Now we can walk SMIL overrride style, without triggering transitions.
      rule->RuleMatched();
      aRuleWalker->Forward(rule);
    }
  }
}

void
nsHTMLCSSStyleSheet::PseudoElementRulesMatching(Element* aPseudoElement,
                                                nsCSSPseudoElements::Type
                                                  aPseudoType,
                                                nsRuleWalker* aRuleWalker)
{
  MOZ_ASSERT(nsCSSPseudoElements::
               PseudoElementSupportsStyleAttribute(aPseudoType));
  MOZ_ASSERT(aPseudoElement);

  // just get the one and only style rule from the content's STYLE attribute
  css::StyleRule* rule = aPseudoElement->GetInlineStyleRule();
  if (rule) {
    rule->RuleMatched();
    aRuleWalker->Forward(rule);
  }
}

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  if (nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(aData->mPseudoType)) {
    MOZ_ASSERT(aData->mPseudoElement,
        "If pseudo element is supposed to support style attribute, it must "
        "have a pseudo element set");

    PseudoElementRulesMatching(aData->mPseudoElement, aData->mPseudoType,
                               aData->mRuleWalker);
  }
}

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

// Test if style is dependent on content state
/* virtual */ nsRestyleHint
nsHTMLCSSStyleSheet::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ nsRestyleHint
nsHTMLCSSStyleSheet::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
nsHTMLCSSStyleSheet::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

// Test if style is dependent on attribute
/* virtual */ nsRestyleHint
nsHTMLCSSStyleSheet::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  // Perhaps should check that it's XUL, SVG, (or HTML) namespace, but
  // it doesn't really matter.
  if (aData->mAttrHasChanged && aData->mAttribute == nsGkAtoms::style) {
    return eRestyle_StyleAttribute;
  }

  return nsRestyleHint(0);
}

/* virtual */ bool
nsHTMLCSSStyleSheet::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

size_t
SizeOfCachedStyleAttrsEntryExcludingThis(nsStringHashKey::KeyType& aKey,
                                         MiscContainer* const& aData,
                                         mozilla::MallocSizeOf aMallocSizeOf,
                                         void* userArg)
{
  // We don't own the MiscContainers so we don't count them. We do care about
  // the size of the nsString members in the keys though.
  return aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

/* virtual */ size_t
nsHTMLCSSStyleSheet::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // The size of mCachedStyleAttrs's mTable member (a PLDHashTable) is
  // significant in itself, but more significant is the size of the nsString
  // members of the nsStringHashKeys.
  return mCachedStyleAttrs.SizeOfExcludingThis(SizeOfCachedStyleAttrsEntryExcludingThis,
                                               aMallocSizeOf,
                                               nullptr);
}

/* virtual */ size_t
nsHTMLCSSStyleSheet::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
nsHTMLCSSStyleSheet::CacheStyleAttr(const nsAString& aSerialized,
                                    MiscContainer* aValue)
{
  mCachedStyleAttrs.Put(aSerialized, aValue);
}

void
nsHTMLCSSStyleSheet::EvictStyleAttr(const nsAString& aSerialized,
                                    MiscContainer* aValue)
{
#ifdef DEBUG
  {
    NS_ASSERTION(aValue = mCachedStyleAttrs.Get(aSerialized),
                 "Cached value does not match?!");
  }
#endif
  mCachedStyleAttrs.Remove(aSerialized);
}

MiscContainer*
nsHTMLCSSStyleSheet::LookupStyleAttr(const nsAString& aSerialized)
{
  return mCachedStyleAttrs.Get(aSerialized);
}
