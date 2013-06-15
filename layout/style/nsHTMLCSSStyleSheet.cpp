/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#include "nsHTMLCSSStyleSheet.h"
#include "mozilla/css/StyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"
#include "nsRuleWalker.h"
#include "nsRuleProcessorData.h"
#include "mozilla/dom/Element.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"

using namespace mozilla::dom;
namespace css = mozilla::css;

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
  mCachedStyleAttrs.Init();
}

nsHTMLCSSStyleSheet::~nsHTMLCSSStyleSheet()
{
  // We may go away before all of our cached style attributes do,
  // so clean up any that are left.
  mCachedStyleAttrs.Enumerate(ClearAttrCache, nullptr);
}

NS_IMPL_ISUPPORTS1(nsHTMLCSSStyleSheet, nsIStyleRuleProcessor)

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  Element* element = aData->mElement;

  // just get the one and only style rule from the content's STYLE attribute
  css::StyleRule* rule = element->GetInlineStyleRule();
  if (rule) {
    rule->RuleMatched();
    aData->mRuleWalker->Forward(rule);
  }

  rule = element->GetSMILOverrideStyleRule();
  if (rule) {
    if (aData->mPresContext->IsProcessingRestyles() &&
        !aData->mPresContext->IsProcessingAnimationStyleChange()) {
      // Non-animation restyle -- don't process SMIL override style, because we
      // don't want SMIL animation to trigger new CSS transitions. Instead,
      // request an Animation restyle, so we still get noticed.
      aData->mPresContext->PresShell()->RestyleForAnimation(element,
                                                            eRestyle_Self);
    } else {
      // Animation restyle (or non-restyle traversal of rules)
      // Now we can walk SMIL overrride style, without triggering transitions.
      rule->RuleMatched();
      aData->mRuleWalker->Forward(rule);
    }
  }
}

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(PseudoElementRuleProcessorData* aData)
{
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
    return eRestyle_Self;
  }

  return nsRestyleHint(0);
}

/* virtual */ bool
nsHTMLCSSStyleSheet::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ size_t
nsHTMLCSSStyleSheet::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return 0;
}

/* virtual */ size_t
nsHTMLCSSStyleSheet::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
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
