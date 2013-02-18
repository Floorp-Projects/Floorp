/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsImageFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIFormControl.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFormControlFrame.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"

using namespace mozilla;

void
IntPointDtorFunc(void *aObject, nsIAtom *aPropertyName,
                 void *aPropertyValue, void *aData)
{
  nsIntPoint *propertyValue = static_cast<nsIntPoint*>(aPropertyValue);
  delete propertyValue;
}


#define nsImageControlFrameSuper nsImageFrame
class nsImageControlFrame : public nsImageControlFrameSuper,
                            public nsIFormControlFrame
{
public:
  nsImageControlFrame(nsStyleContext* aContext);
  ~nsImageControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  virtual nsIAtom* GetType() const;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("ImageControl"), aResult);
  }
#endif

  NS_IMETHOD GetCursor(const nsPoint&    aPoint,
                       nsIFrame::Cursor& aCursor);
  // nsIFormContromFrame
  virtual void SetFocus(bool aOn, bool aRepaint);
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
};


nsImageControlFrame::nsImageControlFrame(nsStyleContext* aContext):
  nsImageControlFrameSuper(aContext)
{
}

nsImageControlFrame::~nsImageControlFrame()
{
}

void
nsImageControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (!GetPrevInFlow()) {
    nsFormControlFrame::RegUnRegAccessKey(this, false);
  }
  nsImageControlFrameSuper::DestroyFrom(aDestructRoot);
}

nsIFrame*
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageControlFrame)

void
nsImageControlFrame::Init(nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIFrame*        aPrevInFlow)
{
  nsImageControlFrameSuper::Init(aContent, aParent, aPrevInFlow);

  if (aPrevInFlow) {
    return;
  }
  
  mContent->SetProperty(nsGkAtoms::imageClickedPoint,
                        new nsIntPoint(0, 0),
                        IntPointDtorFunc);
}

NS_QUERYFRAME_HEAD(nsImageControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsImageControlFrameSuper)

#ifdef ACCESSIBILITY
a11y::AccType
nsImageControlFrame::AccessibleType()
{
  if (mContent->Tag() == nsGkAtoms::button ||
      mContent->Tag() == nsGkAtoms::input) {
    return a11y::eHTMLButtonType;
  }

  return a11y::eNoType;
}
#endif

nsIAtom*
nsImageControlFrame::GetType() const
{
  return nsGkAtoms::imageControlFrame; 
}

NS_METHOD
nsImageControlFrame::Reflow(nsPresContext*         aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (!GetPrevInFlow() && (mState & NS_FRAME_FIRST_REFLOW)) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }
  return nsImageControlFrameSuper::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD 
nsImageControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Don't do anything if the event has already been handled by someone
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // do we have user-input style?
  const nsStyleUserInterface* uiStyle = StyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) { // XXX cache disabled
    return NS_OK;
  }

  *aEventStatus = nsEventStatus_eIgnore;

  if (aEvent->eventStructType == NS_MOUSE_EVENT &&
      aEvent->message == NS_MOUSE_BUTTON_UP &&
      static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton) {
    // Store click point for nsHTMLInputElement::SubmitNamesValues
    // Do this on MouseUp because the specs don't say and that's what IE does
    nsIntPoint* lastClickPoint =
      static_cast<nsIntPoint*>
                 (mContent->GetProperty(nsGkAtoms::imageClickedPoint));
    if (lastClickPoint) {
      // normally lastClickedPoint is not null, as it's allocated in Init()
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
      TranslateEventCoords(pt, *lastClickPoint);
    }
  }
  return nsImageControlFrameSuper::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

void 
nsImageControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

NS_IMETHODIMP
nsImageControlFrame::GetCursor(const nsPoint&    aPoint,
                               nsIFrame::Cursor& aCursor)
{
  // Use style defined cursor if one is provided, otherwise when
  // the cursor style is "auto" we use the pointer cursor.
  FillCursorInformationFromStyle(StyleUserInterface(), aCursor);

  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_POINTER;
  }

  return NS_OK;
}

nsresult
nsImageControlFrame::SetFormProperty(nsIAtom* aName,
                                     const nsAString& aValue)
{
  return NS_OK;
}
