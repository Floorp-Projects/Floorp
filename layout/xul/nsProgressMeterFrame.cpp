/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// David Hyatt & Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsProgressMeterFrame.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsIReflowCallback.h"
#include "nsContentUtils.h"
#include "mozilla/Attributes.h"

class nsReflowFrameRunnable : public mozilla::Runnable
{
public:
  nsReflowFrameRunnable(nsIFrame* aFrame,
                        nsIPresShell::IntrinsicDirty aIntrinsicDirty,
                        nsFrameState aBitToAdd);

  NS_DECL_NSIRUNNABLE

  WeakFrame mWeakFrame;
  nsIPresShell::IntrinsicDirty mIntrinsicDirty;
  nsFrameState mBitToAdd;
};

nsReflowFrameRunnable::nsReflowFrameRunnable(
  nsIFrame* aFrame,
  nsIPresShell::IntrinsicDirty aIntrinsicDirty,
  nsFrameState aBitToAdd)
  : mozilla::Runnable("nsReflowFrameRunnable")
  , mWeakFrame(aFrame)
  , mIntrinsicDirty(aIntrinsicDirty)
  , mBitToAdd(aBitToAdd)
{
}

NS_IMETHODIMP
nsReflowFrameRunnable::Run()
{
  if (mWeakFrame.IsAlive()) {
    mWeakFrame->PresShell()->
      FrameNeedsReflow(mWeakFrame, mIntrinsicDirty, mBitToAdd);
  }
  return NS_OK;
}

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewProgressMeterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsProgressMeterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsProgressMeterFrame)

//
// nsProgressMeterFrame dstr
//
// Cleanup, if necessary
//
nsProgressMeterFrame :: ~nsProgressMeterFrame ( )
{
}

class nsAsyncProgressMeterInit final : public nsIReflowCallback
{
public:
  explicit nsAsyncProgressMeterInit(nsIFrame* aFrame) : mWeakFrame(aFrame) {}

  virtual bool ReflowFinished() override
  {
    bool shouldFlush = false;
    nsIFrame* frame = mWeakFrame.GetFrame();
    if (frame) {
      nsAutoScriptBlocker scriptBlocker;
      frame->AttributeChanged(kNameSpaceID_None, nsGkAtoms::mode, 0);
      shouldFlush = true;
    }
    delete this;
    return shouldFlush;
  }

  virtual void ReflowCallbackCanceled() override
  {
    delete this;
  }

  WeakFrame mWeakFrame;
};

NS_IMETHODIMP
nsProgressMeterFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  if (mNeedsReflowCallback) {
    nsIReflowCallback* cb = new nsAsyncProgressMeterInit(this);
    if (cb) {
      PresShell()->PostReflowCallback(cb);
    }
    mNeedsReflowCallback = false;
  }
  return nsBoxFrame::DoXULLayout(aState);
}

nsresult
nsProgressMeterFrame::AttributeChanged(int32_t aNameSpaceID,
                                       nsAtom* aAttribute,
                                       int32_t aModType)
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
      "Scripts not blocked in nsProgressMeterFrame::AttributeChanged!");
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  if (NS_OK != rv) {
    return rv;
  }

  // did the progress change?
  bool undetermined = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mode,
                                            nsGkAtoms::undetermined, eCaseMatters);
  if (nsGkAtoms::mode == aAttribute ||
      (!undetermined &&
       (nsGkAtoms::value == aAttribute || nsGkAtoms::max == aAttribute))) {
    nsIFrame* barChild = PrincipalChildList().FirstChild();
    if (!barChild || !barChild->GetContent()->IsElement()) return NS_OK;
    nsIFrame* remainderChild = barChild->GetNextSibling();
    if (!remainderChild) return NS_OK;
    nsCOMPtr<nsIContent> remainderContent = remainderChild->GetContent();
    if (!remainderContent->IsElement()) return NS_OK;

    int32_t flex = 1, maxFlex = 1;
    if (!undetermined) {
      nsAutoString value, maxValue;
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::max, maxValue);

      nsresult error;
      flex = value.ToInteger(&error);
      maxFlex = maxValue.ToInteger(&error);
      if (NS_FAILED(error) || maxValue.IsEmpty()) {
        maxFlex = 100;
      }
      if (maxFlex < 1) {
        maxFlex = 1;
      }
      if (flex < 0) {
        flex = 0;
      }
      if (flex > maxFlex) {
        flex = maxFlex;
      }
    }

    nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
      barChild->GetContent()->AsElement(), nsGkAtoms::flex, flex));
    nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
      remainderContent->AsElement(), nsGkAtoms::flex, maxFlex - flex));
    nsContentUtils::AddScriptRunner(new nsReflowFrameRunnable(
      this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY));
  }
  return NS_OK;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsProgressMeterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ProgressMeter"), aResult);
}
#endif
