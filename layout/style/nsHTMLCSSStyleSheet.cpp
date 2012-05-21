/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#include "nsHTMLCSSStyleSheet.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsCSSPseudoElements.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "mozilla/css/StyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "nsRuleProcessorData.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;
namespace css = mozilla::css;

nsHTMLCSSStyleSheet::nsHTMLCSSStyleSheet()
  : mDocument(nsnull)
{
}

NS_IMPL_ISUPPORTS2(nsHTMLCSSStyleSheet,
                   nsIStyleSheet,
                   nsIStyleRuleProcessor)

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

nsresult
nsHTMLCSSStyleSheet::Init(nsIURI* aURL, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURL && aDocument, "null ptr");
  if (! aURL || ! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mURL || mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  mURL = aURL;
  return NS_OK;
}

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
nsHTMLCSSStyleSheet::Reset(nsIURI* aURL)
{
  mURL = aURL;
}

/* virtual */ nsIURI*
nsHTMLCSSStyleSheet::GetSheetURI() const
{
  return mURL;
}

/* virtual */ nsIURI*
nsHTMLCSSStyleSheet::GetBaseURI() const
{
  return mURL;
}

/* virtual */ void
nsHTMLCSSStyleSheet::GetTitle(nsString& aTitle) const
{
  aTitle.AssignLiteral("Internal HTML/CSS Style Sheet");
}

/* virtual */ void
nsHTMLCSSStyleSheet::GetType(nsString& aType) const
{
  aType.AssignLiteral("text/html");
}

/* virtual */ bool
nsHTMLCSSStyleSheet::HasRules() const
{
  // Say we always have rules, since we don't know.
  return true;
}

/* virtual */ bool
nsHTMLCSSStyleSheet::IsApplicable() const
{
  return true;
}

/* virtual */ void
nsHTMLCSSStyleSheet::SetEnabled(bool aEnabled)
{ // these can't be disabled
}

/* virtual */ bool
nsHTMLCSSStyleSheet::IsComplete() const
{
  return true;
}

/* virtual */ void
nsHTMLCSSStyleSheet::SetComplete()
{
}

// style sheet owner info
/* virtual */ nsIStyleSheet*
nsHTMLCSSStyleSheet::GetParentSheet() const
{
  return nsnull;
}

/* virtual */ nsIDocument*
nsHTMLCSSStyleSheet::GetOwningDocument() const
{
  return mDocument;
}

/* virtual */ void
nsHTMLCSSStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;
}

#ifdef DEBUG
/* virtual */ void
nsHTMLCSSStyleSheet::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML CSS Style Sheet: ", out);
  nsCAutoString urlSpec;
  mURL->GetSpec(urlSpec);
  if (!urlSpec.IsEmpty()) {
    fputs(urlSpec.get(), out);
  }
  fputs("\n", out);
}
#endif
