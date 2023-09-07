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

void nsSearchControlFrame::Destroy(DestroyContext& aContext) {
  aContext.AddAnonymousContent(mClearButton.forget());
  nsTextControlFrame::Destroy(aContext);
}

nsresult nsSearchControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // We create an anonymous tree for our input element that is structured as
  // follows:
  //
  // input
  //   div    - placeholder
  //   div    - preview div
  //   div    - editor root
  //   button - clear button
  //
  // If you change this, be careful to change the order of stuff in
  // AppendAnonymousContentTo.

  nsTextControlFrame::CreateAnonymousContent(aElements);

  // FIXME: We could use nsTextControlFrame making the show password buttton
  // code a bit more generic, or rename this frame and use it for password
  // inputs.
  //
  // Create the ::-moz-search-clear-button pseudo-element:
  mClearButton = MakeAnonElement(PseudoStyleType::mozSearchClearButton, nullptr,
                                 nsGkAtoms::button);

  aElements.AppendElement(mClearButton);

  return NS_OK;
}

void nsSearchControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  nsTextControlFrame::AppendAnonymousContentTo(aElements, aFilter);
  if (mClearButton) {
    aElements.AppendElement(mClearButton);
  }
}
