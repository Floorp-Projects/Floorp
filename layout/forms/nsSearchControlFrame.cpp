/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSearchControlFrame.h"

#include "HTMLInputElement.h"
#include "mozilla/PresShell.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSPseudoElements.h"
#include "nsICSSDeclaration.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/AccTypes.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewSearchControlFrame(PresShell* aPresShell,
                                   ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSearchControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSearchControlFrame)

NS_QUERYFRAME_HEAD(nsSearchControlFrame)
  NS_QUERYFRAME_ENTRY(nsSearchControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsTextControlFrame)

nsSearchControlFrame::nsSearchControlFrame(ComputedStyle* aStyle,
                                           nsPresContext* aPresContext)
    : nsTextControlFrame(aStyle, aPresContext, kClassID) {}

void nsSearchControlFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                       PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mClearButton.forget());
  nsTextControlFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult nsSearchControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // We create an anonymous tree for our input element that is structured as
  // follows:
  //
  // input
  //   button - clear button
  //   div    - editor root
  //   div    - placeholder
  //   div    - preview div
  //
  // If you change this, be careful to change the destruction order in
  // nsSearchControlFrame::DestroyFrom.

  // Create the ::-moz-search-clear-button pseudo-element:
  mClearButton = MakeAnonElement(PseudoStyleType::mozSearchClearButton, nullptr,
                                 nsGkAtoms::button);

  aElements.AppendElement(mClearButton);

  nsTextControlFrame::CreateAnonymousContent(aElements);

  // Update clear button visibility based on value
  UpdateClearButtonState();

  return NS_OK;
}

void nsSearchControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mClearButton) {
    aElements.AppendElement(mClearButton);
  }
  nsTextControlFrame::AppendAnonymousContentTo(aElements, aFilter);
}

void nsSearchControlFrame::UpdateClearButtonState() {
  if (!mClearButton) {
    return;
  }

  auto* content = HTMLInputElement::FromNode(mContent);

  nsGenericHTMLElement* element = nsGenericHTMLElement::FromNode(mClearButton);
  nsCOMPtr<nsICSSDeclaration> declaration = element->Style();
  if (content->IsValueEmpty()) {
    declaration->SetProperty("visibility"_ns, "hidden"_ns, ""_ns,
                             IgnoreErrors());
  } else {
    nsAutoCString dummy;
    declaration->RemoveProperty("visibility"_ns, dummy, IgnoreErrors());
  }
}
