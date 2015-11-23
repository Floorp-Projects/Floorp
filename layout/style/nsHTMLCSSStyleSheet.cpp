/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

nsHTMLCSSStyleSheet::nsHTMLCSSStyleSheet()
{
}

nsHTMLCSSStyleSheet::~nsHTMLCSSStyleSheet()
{
  // We may go away before all of our cached style attributes do,
  // so clean up any that are left.
  for (auto iter = mCachedStyleAttrs.Iter(); !iter.Done(); iter.Next()) {
    MiscContainer*& value = iter.Data();

    // Ideally we'd just call MiscContainer::Evict, but we can't do that since
    // we're iterating the hashtable.
    MOZ_ASSERT(value->mType == nsAttrValue::eCSSDeclaration);

    css::Declaration* declaration = value->mValue.mCSSDeclaration;
    declaration->SetHTMLCSSStyleSheet(nullptr);
    value->mValue.mCached = 0;

    iter.Remove();
  }
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
  css::Declaration* declaration = aElement->GetInlineStyleDeclaration();
  if (declaration) {
    declaration->SetImmutable();
    aRuleWalker->Forward(declaration);
  }

  declaration = aElement->GetSMILOverrideStyleDeclaration();
  if (declaration) {
    RestyleManager* restyleManager = aPresContext->RestyleManager();
    if (!restyleManager->SkipAnimationRules()) {
      // Animation restyle (or non-restyle traversal of rules)
      // Now we can walk SMIL overrride style, without triggering transitions.
      declaration->SetImmutable();
      aRuleWalker->Forward(declaration);
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
  css::Declaration* declaration = aPseudoElement->GetInlineStyleDeclaration();
  if (declaration) {
    declaration->SetImmutable();
    aRuleWalker->Forward(declaration);
  }
}

/* virtual */ void
nsHTMLCSSStyleSheet::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  if (nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(aData->mPseudoType) &&
      aData->mPseudoElement) {
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
nsHTMLCSSStyleSheet::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
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

/* virtual */ size_t
nsHTMLCSSStyleSheet::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // The size of mCachedStyleAttrs's mTable member (a PLDHashTable) is
  // significant in itself, but more significant is the size of the nsString
  // members of the nsStringHashKeys.
  size_t n = 0;
  n += mCachedStyleAttrs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mCachedStyleAttrs.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't own the MiscContainers (the hash table values) so we don't
    // count them. We do care about the size of the nsString members in the
    // keys though.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
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
    NS_ASSERTION(aValue == mCachedStyleAttrs.Get(aSerialized),
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
