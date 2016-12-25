/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormControlFrame.h"

#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIDOMHTMLInputElement.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "nsDeviceContext.h"
#include "nsIContent.h"

using namespace mozilla;

//#define FCF_NOISY

nsFormControlFrame::nsFormControlFrame(nsStyleContext* aContext)
  : nsAtomicContainerFrame(aContext)
{
}

nsFormControlFrame::~nsFormControlFrame()
{
}

nsIAtom*
nsFormControlFrame::GetType() const
{
  return nsGkAtoms::formControlFrame;
}

void
nsFormControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Unregister the access key registered in reflow
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsAtomicContainerFrame::DestroyFrom(aDestructRoot);
}

NS_QUERYFRAME_HEAD(nsFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsAtomicContainerFrame)

/* virtual */ nscoord
nsFormControlFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */ nscoord
nsFormControlFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */
LogicalSize
nsFormControlFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                                    WritingMode         aWM,
                                    const LogicalSize&  aCBSize,
                                    nscoord             aAvailableISize,
                                    const LogicalSize&  aMargin,
                                    const LogicalSize&  aBorder,
                                    const LogicalSize&  aPadding,
                                    ComputeSizeFlags    aFlags)
{
  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

nscoord
nsFormControlFrame::GetIntrinsicISize()
{
  // Provide a reasonable default for sites that use an "auto" height.
  // Note that if you change this, you should change the values in forms.css
  // as well.  This is the 13px default width minus the 2px default border.
  return nsPresContext::CSSPixelsToAppUnits(13 - 2 * 2);
}

nscoord
nsFormControlFrame::GetIntrinsicBSize()
{
  // Provide a reasonable default for sites that use an "auto" height.
  // Note that if you change this, you should change the values in forms.css
  // as well. This is the 13px default width minus the 2px default border.
  return nsPresContext::CSSPixelsToAppUnits(13 - 2 * 2);
}

nscoord
nsFormControlFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  NS_ASSERTION(!NS_SUBTREE_DIRTY(this),
               "frame must not be dirty");

// NOTE: on Android we use appearance:none by default for checkbox/radio,
// so the different layout for appearance:none we have on other platforms
// doesn't work there. *shrug*
#if !defined(MOZ_WIDGET_ANDROID)
  // For appearance:none we use a standard CSS baseline, i.e. synthesized from
  // our margin-box.
  if (StyleDisplay()->mAppearance == NS_THEME_NONE) {
    return nsAtomicContainerFrame::GetLogicalBaseline(aWritingMode);
  }
#endif

  // This is for compatibility with Chrome, Safari and Edge (Dec 2016).
  // Treat radio buttons and checkboxes as having an intrinsic baseline
  // at the block-end of the control (use the block-end content edge rather
  // than the margin edge).
  // For "inverted" lines (typically in writing-mode:vertical-lr), use the
  // block-start end instead.
  return aWritingMode.IsLineInverted()
    ? GetLogicalUsedBorderAndPadding(aWritingMode).BStart(aWritingMode)
    : BSize(aWritingMode) -
         GetLogicalUsedBorderAndPadding(aWritingMode).BEnd(aWritingMode);
}

void
nsFormControlFrame::Reflow(nsPresContext*          aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFormControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsFormControlFrame::Reflow: aMaxSize=%d,%d",
                  aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  if (mState & NS_FRAME_FIRST_REFLOW) {
    RegUnRegAccessKey(static_cast<nsIFrame*>(this), true);
  }

  aStatus = NS_FRAME_COMPLETE;
  aDesiredSize.SetSize(aReflowInput.GetWritingMode(),
                       aReflowInput.ComputedSizeWithBorderPadding());

  if (nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
    aDesiredSize.Width() *= inflation;
    aDesiredSize.Height() *= inflation;
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsFormControlFrame::Reflow: size=%d,%d",
                  aDesiredSize.Width(), aDesiredSize.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);
}

nsresult
nsFormControlFrame::RegUnRegAccessKey(nsIFrame * aFrame, bool aDoReg)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  
  nsPresContext* presContext = aFrame->PresContext();
  
  NS_ASSERTION(presContext, "aPresContext is NULL in RegUnRegAccessKey!");

  nsAutoString accessKey;

  nsIContent* content = aFrame->GetContent();
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  if (!accessKey.IsEmpty()) {
    EventStateManager* stateManager = presContext->EventStateManager();
    if (aDoReg) {
      stateManager->RegisterAccessKey(content, (uint32_t)accessKey.First());
    } else {
      stateManager->UnregisterAccessKey(content, (uint32_t)accessKey.First());
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void 
nsFormControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

nsresult
nsFormControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                WidgetGUIEvent* aEvent,
                                nsEventStatus* aEventStatus)
{
  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle = StyleUserInterface();
  if (uiStyle->mUserInput == StyleUserInput::None ||
      uiStyle->mUserInput == StyleUserInput::Disabled) {
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
  return NS_OK;
}

void
nsFormControlFrame::GetCurrentCheckState(bool *aState)
{
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    inputElement->GetChecked(aState);
  }
}

nsresult
nsFormControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  return NS_OK;
}

// static
nsRect
nsFormControlFrame::GetUsableScreenRect(nsPresContext* aPresContext)
{
  nsRect screen;

  nsDeviceContext *context = aPresContext->DeviceContext();
  int32_t dropdownCanOverlapOSBar =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenusCanOverlapOSBar, 0);
  if ( dropdownCanOverlapOSBar )
    context->GetRect(screen);
  else
    context->GetClientRect(screen);

  return screen;
}
