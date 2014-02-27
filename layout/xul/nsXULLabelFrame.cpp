/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* derived class of nsBlockFrame used for xul:label elements */

#include "nsXULLabelFrame.h"
#include "nsHTMLParts.h"
#include "nsNameSpaceManager.h"
#include "nsEventStateManager.h"

nsIFrame*
NS_NewXULLabelFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsXULLabelFrame* it = new (aPresShell) nsXULLabelFrame(aContext);
  
  it->SetFlags(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULLabelFrame)

// If you make changes to this function, check its counterparts 
// in nsBoxFrame and nsTextBoxFrame
nsresult
nsXULLabelFrame::RegUnregAccessKey(bool aDoReg)
{
  // if we have no content, we can't do anything
  if (!mContent)
    return NS_ERROR_FAILURE;

  // To filter out <label>s without a control attribute.
  // XXXjag a side-effect is that we filter out anonymous <label>s
  // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
  // |accesskey| and would otherwise register themselves, overwriting
  // the content we really meant to be registered.
  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::control))
    return NS_OK;

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return NS_OK;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  nsEventStateManager *esm = PresContext()->EventStateManager();

  uint32_t key = accessKey.First();
  if (aDoReg)
    esm->RegisterAccessKey(mContent, key);
  else
    esm->UnregisterAccessKey(mContent, key);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

void
nsXULLabelFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  // register access key
  RegUnregAccessKey(true);
}

void
nsXULLabelFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // unregister access key
  RegUnregAccessKey(false);
  nsBlockFrame::DestroyFrom(aDestructRoot);
} 

nsresult
nsXULLabelFrame::AttributeChanged(int32_t aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t aModType)
{
  nsresult rv = nsBlockFrame::AttributeChanged(aNameSpaceID, 
                                               aAttribute, aModType);

  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  if (aAttribute == nsGkAtoms::accesskey || aAttribute == nsGkAtoms::control)
    RegUnregAccessKey(true);

  return rv;
}

nsIAtom*
nsXULLabelFrame::GetType() const
{
  return nsGkAtoms::XULLabelFrame;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef DEBUG_FRAME_DUMP
nsresult
nsXULLabelFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULLabel"), aResult);
}
#endif
