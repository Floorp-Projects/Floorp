/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIImageControlFrame.h"
#include "nsImageFrame.h"
#include "nsFormControlHelper.h"
#include "nsIFormControlFrame.h"
#include "nsIFormControl.h"
#include "nsHTMLParts.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIImage.h"
#include "nsStyleConsts.h"
#include "nsFormControlFrame.h"
#include "nsGUIEvent.h"
#include "nsLayoutAtoms.h"
#include "nsIServiceManager.h"
#include "nsContainerFrame.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

//Enumeration of possible mouse states used to detect mouse clicks
/*enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
  eMouseDown,
  eMouseUp
};
*/
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

#define nsImageControlFrameSuper nsImageFrame
class nsImageControlFrame : public nsImageControlFrameSuper,
                            public nsIFormControlFrame,
                            public nsIImageControlFrame
{
public:
  nsImageControlFrame();
  ~nsImageControlFrame();

  NS_IMETHOD Destroy(nsIPresContext *aPresContext);
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  virtual nsIAtom* GetType() const;

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("ImageControl"), aResult);
  }
#endif

  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                       nsPoint&        aPoint,
                       PRInt32&        aCursor);

  virtual void MouseClicked(nsIPresContext* aPresContext);

  NS_IMETHOD_(PRInt32) GetFormControlType() const;

  NS_IMETHOD GetName(nsAString* aName);

  void SetFocus(PRBool aOn, PRBool aRepaint);
  void ScrollIntoView(nsIPresContext* aPresContext);
  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                     const nsFont*&  aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


        // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAString& aValue); 
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD OnContentReset();

  // nsIImageControlFrame
  NS_IMETHOD GetClickedX(PRInt32* aX);
  NS_IMETHOD GetClickedY(PRInt32* aY);

protected:
  void GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect); // XXX this implementation is a copy of nsHTMLButtonControlFrame
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  nsMouseState mLastMouseState;
  nsPoint mLastClickPoint; 
  nsCursor mPreviousCursor;
  nsRect mTranslatedRect;
  PRBool mGotFocus;

};


nsImageControlFrame::nsImageControlFrame()
{
  mLastMouseState = eMouseNone;
  mLastClickPoint = nsPoint(0,0);
  mPreviousCursor = eCursor_standard;
  mTranslatedRect = nsRect(0,0,0,0);
  mGotFocus       = PR_FALSE;
}

nsImageControlFrame::~nsImageControlFrame()
{
}

NS_IMETHODIMP
nsImageControlFrame::Destroy(nsIPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);

  return nsImageControlFrameSuper::Destroy(aPresContext);
}

nsresult
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsImageControlFrame* it = new (aPresShell) nsImageControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsImageControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } 
  if (aIID.Equals(NS_GET_IID(nsIImageControlFrame))) {
    *aInstancePtr = (void*) ((nsIImageControlFrame*) this);
    return NS_OK;
  }

  return nsImageControlFrameSuper::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsImageControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTML4ButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

nsrefcnt nsImageControlFrame::AddRef(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsrefcnt nsImageControlFrame::Release(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsIAtom*
nsImageControlFrame::GetType() const
{
  return nsLayoutAtoms::imageControlFrame; 
}

NS_IMETHODIMP
nsImageControlFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsStyleContext*  aContext,
                          nsIFrame*        aPrevInFlow)
{
  // call our base class
  nsresult  rv = nsImageControlFrameSuper::Init(aPresContext, aContent, aParent,
                                                aContext, aPrevInFlow);
  
  // create our view, we need a view to grab the mouse 
  if (!HasView()) {
    nsIView* view;
    nsresult result = CallCreateInstance(kViewCID, &view);
    nsIViewManager* viewMan = aPresContext->GetViewManager();

    nsIFrame* parWithView = GetAncestorWithView();
    nsIView *parView = parWithView->GetView();
    // the view's size is not know yet, but its size will be kept in synch with our frame.
    nsRect boundBox(0, 0, 0, 0); 
    result = view->Init(viewMan, boundBox, parView);

    nsContainerFrame::SyncFrameViewProperties(aPresContext, this, aContext, view);

    // this gets reset during reflow anyway
    // viewMan->SetViewContentTransparency(view, PR_TRUE);

    // XXX put the view last in document order until we know how to do better
    viewMan->InsertChild(parView, view, nsnull, PR_TRUE);
    SetView(view);
  }

  return rv;
}

NS_METHOD
nsImageControlFrame::Reflow(nsIPresContext*         aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (aReflowState.reason == eReflowReason_Initial) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  }
  return nsImageControlFrameSuper::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD 
nsImageControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Don't do anything if the event has already been handled by someone
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // do we have user-input style?
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (nsFormControlHelper::GetDisabled(mContent)) { // XXX cache disabled
    return NS_OK;
  }

  *aEventStatus = nsEventStatus_eIgnore;

  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
    {
      // Store click point for GetNamesValues
      // Do this on MouseUp because the specs don't say and that's what IE does.
      float t2p;
      t2p = aPresContext->TwipsToPixels();
      mLastClickPoint.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
      mLastClickPoint.y = NSTwipsToIntPixels(aEvent->point.y, t2p);

      mGotFocus = PR_TRUE;
      break;
    }
  }
  return nsImageControlFrameSuper::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

void 
nsImageControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mGotFocus = aOn;
  /*if (aRepaint) {
    nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
  }*/
}

void
nsImageControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}

void
nsImageControlFrame::GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect)
{
  nsIView* view;
  nsPoint viewOffset(0,0);
  GetOffsetFromView(aPresContext, viewOffset, &view);
  while (view) {
    viewOffset += view->GetPosition();
    view = view->GetParent();
  }
  aRect = nsRect(viewOffset.x, viewOffset.y, mRect.width, mRect.height);
}

NS_IMETHODIMP_(PRInt32)
nsImageControlFrame::GetFormControlType() const
{
  return NS_FORM_INPUT_IMAGE;
}

NS_IMETHODIMP
nsImageControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}

NS_IMETHODIMP
nsImageControlFrame::GetCursor(nsIPresContext* aPresContext,
                               nsPoint&        aPoint,
                               PRInt32&        aCursor)
{
  // Use style defined cursor if one is provided, otherwise when
  // the cursor style is "auto" we use the pointer cursor.
  aCursor = GetStyleUserInterface()->mCursor;
  if (NS_STYLE_CURSOR_AUTO == aCursor) {
    aCursor = NS_STYLE_CURSOR_POINTER;
  }
  return NS_OK;
}

void
nsImageControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
// This is no longer called; click events are handled in
// nsHTMLInputElement::HandleDOMEvent().
}

NS_IMETHODIMP
nsImageControlFrame::GetFont(nsIPresContext* aPresContext, 
                             const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}

NS_IMETHODIMP
nsImageControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

nscoord 
nsImageControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                              float aPixToTwip, 
                                              nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsImageControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

nsresult nsImageControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsImageControlFrame::SetProperty(nsIPresContext* aPresContext,
                                               nsIAtom* aName,
                                               const nsAString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageControlFrame::GetProperty(nsIAtom* aName,
                                               nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsImageControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
//  mSuggestedWidth = aWidth;
//  mSuggestedHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::OnContentReset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetClickedX(PRInt32* aX)
{
  *aX = mLastClickPoint.x;
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetClickedY(PRInt32* aY)
{
  *aY = mLastClickPoint.y;
  return NS_OK;
}
