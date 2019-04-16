/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* derived class of nsBlockFrame used for xul:label elements */

#include "nsXULLabelFrame.h"

#include "mozilla/EventStateManager.h"
#include "mozilla/PresShell.h"
#include "nsHTMLParts.h"
#include "nsNameSpaceManager.h"

using namespace mozilla;

nsIFrame* NS_NewXULLabelFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  nsXULLabelFrame* it =
      new (aPresShell) nsXULLabelFrame(aStyle, aPresShell->GetPresContext());
  it->AddStateBits(NS_BLOCK_FORMATTING_CONTEXT_STATE_BITS);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULLabelFrame)

// If you make changes to this function, check its counterparts
// in nsBoxFrame and nsTextBoxFrame
nsresult nsXULLabelFrame::RegUnregAccessKey(bool aDoReg) {
  // To filter out <label>s without a control attribute.
  // XXXjag a side-effect is that we filter out anonymous <label>s
  // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
  // |accesskey| and would otherwise register themselves, overwriting
  // the content we really meant to be registered.
  if (!mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::control))
    return NS_OK;

  nsAutoString accessKey;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                 accessKey);

  if (accessKey.IsEmpty()) return NS_OK;

  // With a valid PresContext we can get the ESM
  // and register the access key
  EventStateManager* esm = PresContext()->EventStateManager();

  uint32_t key = accessKey.First();
  if (aDoReg)
    esm->RegisterAccessKey(mContent->AsElement(), key);
  else
    esm->UnregisterAccessKey(mContent->AsElement(), key);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

void nsXULLabelFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  // register access key
  RegUnregAccessKey(true);
}

void nsXULLabelFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                  PostDestroyData& aPostDestroyData) {
  // unregister access key
  RegUnregAccessKey(false);
  nsBlockFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult nsXULLabelFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  nsresult rv =
      nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  if (aAttribute == nsGkAtoms::accesskey || aAttribute == nsGkAtoms::control)
    RegUnregAccessKey(true);

  return rv;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef DEBUG_FRAME_DUMP
nsresult nsXULLabelFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("XULLabel"), aResult);
}
#endif
