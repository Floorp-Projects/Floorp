/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifdef MOZ_SMIL
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
#endif // MOZ_SMIL
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
  return PR_FALSE;
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
  return PR_FALSE;
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
  return PR_TRUE;
}

/* virtual */ bool
nsHTMLCSSStyleSheet::IsApplicable() const
{
  return PR_TRUE;
}

/* virtual */ void
nsHTMLCSSStyleSheet::SetEnabled(bool aEnabled)
{ // these can't be disabled
}

/* virtual */ bool
nsHTMLCSSStyleSheet::IsComplete() const
{
  return PR_TRUE;
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
