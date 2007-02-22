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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsSplitterFrame.h"
#include "nsGkAtoms.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMDocument.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsHTMLParts.h"
#include "nsILookAndFeel.h"
#include "nsStyleContext.h"
#include "nsWidgetsCID.h"
#include "nsBoxLayoutState.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsGUIEvent.h"
#include "nsAutoPtr.h"
#include "nsContentCID.h"
#include "nsStyleSet.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"

// was used in nsSplitterFrame::Init but now commented out
//static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
PRInt32 realTimeDrag;

class nsSplitterInfo {
public:
  nscoord min;
  nscoord max;
  nscoord current;
  nscoord changed;
  nsCOMPtr<nsIContent> childElem;
  PRInt32 flex;
  PRInt32 index;
};

class nsSplitterFrameInner : public nsIDOMMouseListener, public nsIDOMMouseMotionListener {

public:

  NS_DECL_ISUPPORTS

  nsSplitterFrameInner(nsSplitterFrame* aSplitter)
  {
    mOuter = aSplitter;
    mPressed = PR_FALSE;
  }
  virtual ~nsSplitterFrameInner();

  // mouse listener
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) { return MouseMove(aMouseEvent); }
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  // mouse motion listener
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  void MouseDrag(nsPresContext* aPresContext, nsGUIEvent* aEvent);
  void MouseUp(nsPresContext* aPresContext, nsGUIEvent* aEvent);

  void AdjustChildren(nsPresContext* aPresContext);
  void AdjustChildren(nsPresContext* aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal);

  void AddRemoveSpace(nscoord aDiff,
                    nsSplitterInfo* aChildInfos,
                    PRInt32 aCount,
                    PRInt32& aSpaceLeft);

  void ResizeChildTo(nsPresContext* aPresContext,
                   nscoord& aDiff, 
                   nsSplitterInfo* aChildrenBeforeInfos, 
                   nsSplitterInfo* aChildrenAfterInfos, 
                   PRInt32 aChildrenBeforeCount, 
                   PRInt32 aChildrenAfterCount, 
                   PRBool aBounded);

  void UpdateState();

  void AddListener(nsPresContext* aPresContext);
  void RemoveListener();

  enum ResizeType { Closest, Farthest, Grow };
  enum State { Open, CollapsedBefore, CollapsedAfter, Dragging };
  enum CollapseDirection { Before, After };

  ResizeType GetResizeBefore();
  ResizeType GetResizeAfter();
  State GetState();

  //nsresult CaptureMouse(nsPresContext* aPresContext, PRBool aGrabMouseEvents);
  //PRBool IsMouseCaptured(nsPresContext* aPresContext);
  void Reverse(nsSplitterInfo*& aIndexes, PRInt32 aCount);
  PRBool SupportsCollapseDirection(CollapseDirection aDirection);

  void MoveSplitterBy(nsPresContext* aPresContext, nscoord aDiff);
  void EnsureOrient();
  void SetPreferredSize(nsBoxLayoutState& aState, nsIBox* aChildBox, nscoord aOnePixel, PRBool aIsHorizontal, nscoord* aSize);

  nsSplitterFrame* mOuter;
  PRBool mDidDrag;
  nscoord mDragStart;
  nscoord mCurrentPos;
  nsIBox* mParentBox;
  PRBool mPressed;
  nsSplitterInfo* mChildInfosBefore;
  nsSplitterInfo* mChildInfosAfter;
  PRInt32 mChildInfosBeforeCount;
  PRInt32 mChildInfosAfterCount;
  State mState;
  nscoord mSplitterPos;
  nscoord mSplitterViewPos;
  PRBool mDragging;

};


NS_IMPL_ISUPPORTS2(nsSplitterFrameInner, nsIDOMMouseListener, nsIDOMMouseMotionListener)

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeBefore()
{
  if (mOuter->GetContent()->
        AttrValueIs(kNameSpaceID_None, nsGkAtoms::resizebefore,
                    NS_LITERAL_STRING("farthest"), eCaseMatters))
    return Farthest;
  return Closest;
}

nsSplitterFrameInner::~nsSplitterFrameInner() 
{
  delete[] mChildInfosBefore;
  delete[] mChildInfosAfter;
}

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeAfter()
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::farthest, &nsGkAtoms::grow, nsnull};
  switch (mOuter->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::resizeafter,
                                                strings, eCaseMatters)) {
    case 0: return Farthest;
    case 1: return Grow;
  }
  return Closest;
}

nsSplitterFrameInner::State
nsSplitterFrameInner::GetState()
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::dragging, &nsGkAtoms::collapsed, nsnull};
  static nsIContent::AttrValuesArray strings_substate[] =
    {&nsGkAtoms::before, &nsGkAtoms::after, nsnull};
  switch (mOuter->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::state,
                                                strings, eCaseMatters)) {
    case 0: return Dragging;
    case 1:
      switch (mOuter->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                    nsGkAtoms::substate,
                                                    strings_substate,
                                                    eCaseMatters)) {
        case 0: return CollapsedBefore;
        case 1: return CollapsedAfter;
        default:
          if (SupportsCollapseDirection(After))
            return CollapsedAfter;
          return CollapsedBefore;
      }
  }
  return Open;
}

//
// NS_NewSplitterFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewSplitterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSplitterFrame(aPresShell, aContext);
} // NS_NewSplitterFrame

nsSplitterFrame::nsSplitterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
: nsBoxFrame(aPresShell, aContext),
  mInner(0)
{
}

nsSplitterFrame::~nsSplitterFrame()
{
  if (mInner) {
    mInner->RemoveListener();
    mInner->Release();
  }
}


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsSplitterFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


NS_IMETHODIMP
nsSplitterFrame::GetCursor(const nsPoint&    aPoint,
                           nsIFrame::Cursor& aCursor)
{
  return nsBoxFrame::GetCursor(aPoint, aCursor);

  /*
    if (IsHorizontal())
      aCursor = NS_STYLE_CURSOR_N_RESIZE;
    else
      aCursor = NS_STYLE_CURSOR_W_RESIZE;

    return NS_OK;
  */
}

NS_IMETHODIMP
nsSplitterFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  // if the alignment changed. Let the grippy know
  if (aAttribute == nsGkAtoms::align) {
    // tell the slider its attribute changed so it can 
    // update itself
    nsIFrame* grippy = nsnull;
    nsScrollbarButtonFrame::GetChildWithTag(GetPresContext(), nsGkAtoms::grippy, this, grippy);
    if (grippy)
      grippy->AttributeChanged(aNameSpaceID, aAttribute, aModType);
  } else if (aAttribute == nsGkAtoms::state) {
    mInner->UpdateState();
  }

  return rv;
}

/**
 * Initialize us. If we are in a box get our alignment so we know what direction we are
 */
NS_IMETHODIMP
nsSplitterFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  NS_ENSURE_FALSE(mInner, NS_ERROR_ALREADY_INITIALIZED);
  mInner = new nsSplitterFrameInner(this);
  if (!mInner)
    return NS_ERROR_OUT_OF_MEMORY;

  mInner->AddRef();
  mInner->mChildInfosAfter = nsnull;
  mInner->mChildInfosBefore = nsnull;
  mInner->mState = nsSplitterFrameInner::Open;
  mInner->mDragging = PR_FALSE;

  {
#if 0
    // make it real time drag for now due to problems
    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
    if (lookAndFeel) {
      lookAndFeel->GetMetric(nsILookAndFeel::eMetric_DragFullWindow, realTimeDrag);
    }
    else
#endif
      realTimeDrag = 1;
  }

  // determine orientation of parent, and if vertical, set orient to vertical
  // on splitter content, then re-resolve style
  // |newContext| to Release the reference after the call to nsBoxFrame::Init
  nsRefPtr<nsStyleContext> newContext;
  if (aParent && aParent->IsBoxFrame()) {
    if (!aParent->IsHorizontal()) {
      if (!nsContentUtils::HasNonEmptyAttr(aContent, kNameSpaceID_None,
                                           nsGkAtoms::orient)) {
        aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                          NS_LITERAL_STRING("vertical"), PR_FALSE);
        nsStyleContext* parentStyleContext = aParent->GetStyleContext();
        newContext = GetStyleContext()->GetRuleNode()->GetPresContext()->StyleSet()->
          ResolveStyleFor(aContent, parentStyleContext);
        SetStyleContextWithoutNotification(newContext);
      }
    }
  }

  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  nsHTMLContainerFrame::CreateViewForFrame(this, nsnull, PR_TRUE);
  nsIView* view = GetView();

  nsIViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewContentTransparency(view, PR_TRUE);

  if (!realTimeDrag) {
    // currently this only works on win32 and mac
    static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

    // Need to have a widget to appear on top of other widgets.
    view->CreateWidget(kCChildCID);
  }

  mInner->mState = nsSplitterFrameInner::Open;
  mInner->AddListener(GetPresContext());
  mInner->mParentBox = nsnull;
  return rv;
}

NS_IMETHODIMP
nsSplitterFrame::DoLayout(nsBoxLayoutState& aState)
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) 
  {
    mInner->mParentBox = GetParentBox();
    mInner->UpdateState();
  }

  return nsBoxFrame::DoLayout(aState);
}


void
nsSplitterFrame::GetInitialOrientation(PRBool& aIsHorizontal)
{
  nsIBox* box = GetParentBox();
  if (box) {
    aIsHorizontal = !box->IsHorizontal();
  }
  else
    nsBoxFrame::GetInitialOrientation(aIsHorizontal);
}

NS_IMETHODIMP
nsSplitterFrame::HandlePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleMultiplePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleDrag(nsPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleRelease(nsPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  nsresult rv = nsBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // if the mouse is captured always return us as the frame.
  if (mInner->mDragging)
  {
    // XXX It's probably better not to check visibility here, right?
    return aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayEventReceiver(this));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  nsWeakFrame weakFrame(this);
  nsRefPtr<nsSplitterFrameInner> kungFuDeathGrip(mInner);
  switch (aEvent->message) {
    case NS_MOUSE_MOVE: 
      mInner->MouseDrag(aPresContext, aEvent);
    break;
  
    case NS_MOUSE_BUTTON_UP:
      if (aEvent->eventStructType == NS_MOUSE_EVENT &&
          NS_STATIC_CAST(nsMouseEvent*, aEvent)->button ==
            nsMouseEvent::eLeftButton) {
        mInner->MouseUp(aPresContext, aEvent);
      }
    break;
  }

  NS_ENSURE_STATE(weakFrame.IsAlive());
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void
nsSplitterFrameInner::MouseUp(nsPresContext* aPresContext, nsGUIEvent* aEvent)
{
  if (mDragging) {
    AdjustChildren(aPresContext);
    AddListener(aPresContext);
    mOuter->CaptureMouse(aPresContext, PR_FALSE);
    mDragging = PR_FALSE;
    State newState = GetState(); 
    // if the state is dragging then make it Open.
    if (newState == Dragging)
      mOuter->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::state, EmptyString(), PR_TRUE);

    mPressed = PR_FALSE;

    // if we dragged then fire a command event.
    if (mDidDrag) {
      nsCOMPtr<nsIDOMXULElement> element = do_QueryInterface(mOuter->GetContent());
      element->DoCommand();
    }

    //printf("MouseUp\n");
  }

  delete[] mChildInfosBefore;
  delete[] mChildInfosAfter;
  mChildInfosBefore = nsnull;
  mChildInfosAfter = nsnull;
}

void
nsSplitterFrameInner::MouseDrag(nsPresContext* aPresContext, nsGUIEvent* aEvent)
{
  if (mDragging) {

    //printf("Dragging\n");

    PRBool isHorizontal = !mOuter->IsHorizontal();
    // convert coord to pixels
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                              mParentBox);
    nscoord pos = isHorizontal ? pt.x : pt.y;

    // mDragStart is in frame coordinates
    nscoord start = mDragStart;

    // take our current position and subtract the start location
    pos -= start;

    //printf("Diff=%d\n", pos);

    ResizeType resizeAfter  = GetResizeAfter();

    PRBool bounded;

    if (resizeAfter == nsSplitterFrameInner::Grow)
      bounded = PR_FALSE;
    else 
      bounded = PR_TRUE;

    int i;
    for (i=0; i < mChildInfosBeforeCount; i++) 
      mChildInfosBefore[i].changed = mChildInfosBefore[i].current;

    for (i=0; i < mChildInfosAfterCount; i++) 
      mChildInfosAfter[i].changed = mChildInfosAfter[i].current;

    nscoord oldPos = pos;

    ResizeChildTo(aPresContext, pos, mChildInfosBefore, mChildInfosAfter, mChildInfosBeforeCount, mChildInfosAfterCount, bounded);

    State currentState = GetState();
    PRBool supportsBefore = SupportsCollapseDirection(Before);
    PRBool supportsAfter = SupportsCollapseDirection(After);

    // if we are in a collapsed position
    if (realTimeDrag && ((oldPos > 0 && oldPos > pos && supportsAfter) ||
                         (oldPos < 0 && oldPos < pos && supportsBefore)))
    {
      // and we are not collapsed then collapse
      if (currentState == Dragging) {
        if (oldPos > 0 && oldPos > pos)
        {
          //printf("Collapse right\n");
          if (supportsAfter) 
          {
            nsCOMPtr<nsIContent> outer = mOuter->mContent;
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate,
                           NS_LITERAL_STRING("after"),
                           PR_TRUE);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                           NS_LITERAL_STRING("collapsed"),
                           PR_TRUE);
          }

        } else if (oldPos < 0 && oldPos < pos)
        {
          //printf("Collapse left\n");
          if (supportsBefore)
          {
            nsCOMPtr<nsIContent> outer = mOuter->mContent;
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate,
                           NS_LITERAL_STRING("before"),
                           PR_TRUE);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                           NS_LITERAL_STRING("collapsed"),
                           PR_TRUE);
          }
        }
      }
    } else {
      // if we are not in a collapsed position and we are not dragging make sure
      // we are dragging.
      if (currentState != Dragging)
        mOuter->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::state, NS_LITERAL_STRING("dragging"), PR_TRUE);
      if (realTimeDrag)
        AdjustChildren(aPresContext);
      else
        MoveSplitterBy(aPresContext, pos);
    }

    // printf("----- resize ----- ");
    /*
      for (i=0; i < mChildInfosBeforeCount; i++) 
        printf("before, index=%d, current=%d, changed=%d\n", mChildInfosBefore[i].index, mChildInfosBefore[i].current, mChildInfosBefore[i].changed);
      for (i=0; i < mChildInfosAfterCount; i++) 
        printf("after, index=%d, current=%d, changed=%d\n", mChildInfosAfter[i].index, mChildInfosAfter[i].current, mChildInfosAfter[i].changed);
    */

    /*
      nsIPresShell *shell = aPresContext->PresShell();

      mOuter->mState |= NS_FRAME_IS_DIRTY;
      shell->FrameNeedsReflow(mOuter, nsIPresShell::eStyleChange);
    */
    mDidDrag = PR_TRUE;
  }
}

void
nsSplitterFrameInner::AddListener(nsPresContext* aPresContext)
{
  nsCOMPtr<nsIDOMEventReceiver>
    receiver(do_QueryInterface(mOuter->GetContent()));

  receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this), NS_GET_IID(nsIDOMMouseListener));
  receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this), NS_GET_IID(nsIDOMMouseMotionListener));
}

void
nsSplitterFrameInner::RemoveListener()
{
  nsCOMPtr<nsIDOMEventReceiver>
    receiver(do_QueryInterface(mOuter->GetContent()));

  receiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this),NS_GET_IID(nsIDOMMouseListener));
  receiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this),NS_GET_IID(nsIDOMMouseMotionListener));
}

/*
nsresult
nsSplitterFrameInner :: CaptureMouse(nsPresContext* aPresContext, PRBool aGrabMouseEvents)
{
  // get its view
  nsIView* view = mOuter->GetView();
  PRBool result;

  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();
    if (viewMan) {
      // nsIWidget* widget = view->GetWidget();
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
        //  if (widget)
        //   widget->CaptureMouse(PR_TRUE);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
       // if (widget)
         //  widget->CaptureMouse(PR_FALSE);
      }
    }
  }

  return NS_OK;
}


PRBool
nsSplitterFrameInner :: IsMouseCaptured(nsPresContext* aPresContext)
{
    // get its view
  nsIView* view = mOuter->GetView();
  
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
}
*/

nsresult
nsSplitterFrameInner::MouseUp(nsIDOMEvent* aMouseEvent)
{  
  mPressed = PR_FALSE;

  mOuter->CaptureMouse(mOuter->GetPresContext(), PR_FALSE);

  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseDown(nsIDOMEvent* aMouseEvent)
{  
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));

  PRUint16 button = 0;
  mouseEvent->GetButton(&button);

  // only if left button
  if (button != 0)
     return NS_OK;

  if (mOuter->GetContent()->
        AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                    nsGkAtoms::_true, eCaseMatters))
    return NS_OK;

  mParentBox = mOuter->GetParentBox();
  if (!mParentBox)
    return NS_OK;
  
  // get our index
  nsPresContext* outerPresContext = mOuter->GetPresContext();
  nscoord childIndex = nsFrameNavigator::IndexOf(outerPresContext, mParentBox, mOuter);
  PRInt32 childCount = nsFrameNavigator::CountFrames(outerPresContext, mParentBox);

  // if it's 0 or the last index then stop right here.
  if (childIndex == 0 || childIndex == childCount - 1)
    return NS_OK;

  // XXXbz using this state for GetPrefSize/GetMinSize is wrong.  It
  // needs a rendering context!
  nsBoxLayoutState state(outerPresContext);
  mCurrentPos = 0;
  mPressed = PR_TRUE;

  mDidDrag = PR_FALSE;

  EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();
  
  ResizeType resizeBefore = GetResizeBefore();
  ResizeType resizeAfter  = GetResizeAfter();

  delete[] mChildInfosBefore;
  delete[] mChildInfosAfter;
  mChildInfosBefore = new nsSplitterInfo[childCount];
  mChildInfosAfter  = new nsSplitterInfo[childCount];

  // create info 2 lists. One of the children before us and one after.
  PRInt32 count = 0;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;

  nsIBox* childBox = mParentBox->GetChildBox();

  while (nsnull != childBox) 
  { 
    nsIContent* content = childBox->GetContent();
    nsCOMPtr<nsIAtom> atom;
    nsresult rv;
    nsCOMPtr<nsIXBLService> xblService = 
             do_GetService("@mozilla.org/xbl;1", &rv);

    if (NS_SUCCEEDED(rv) && xblService) {
      PRInt32 dummy;
      xblService->ResolveTag(content, &dummy, getter_AddRefs(atom));
    } else
      atom = content->Tag();

    // skip over any splitters
    if (atom != nsGkAtoms::splitter) { 
        nsSize prefSize = childBox->GetPrefSize(state);
        nsSize minSize = childBox->GetMinSize(state);
        nsSize maxSize = childBox->GetMaxSize(state);
        nsBox::BoundsCheck(minSize, prefSize, maxSize);

        mOuter->AddMargin(childBox, minSize);
        mOuter->AddMargin(childBox, prefSize);
        mOuter->AddMargin(childBox, maxSize);

        nscoord flex = childBox->GetFlex(state);

        nsMargin margin(0,0,0,0);
        childBox->GetMargin(margin);
        nsRect r(childBox->GetRect());
        r.Inflate(margin);

        // We need to check for hidden attribute too, since treecols with
        // the hidden="true" attribute are not really hidden, just collapsed
        if (!content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::fixed,
                                  nsGkAtoms::_true, eCaseMatters) &&
            !content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                  nsGkAtoms::_true, eCaseMatters)) {
            if (count < childIndex) {
                mChildInfosBefore[mChildInfosBeforeCount].childElem = content;
                mChildInfosBefore[mChildInfosBeforeCount].min     = isHorizontal ? minSize.width : minSize.height;
                mChildInfosBefore[mChildInfosBeforeCount].max     = isHorizontal ? maxSize.width : maxSize.height;
                mChildInfosBefore[mChildInfosBeforeCount].current = isHorizontal ? r.width : r.height;
                mChildInfosBefore[mChildInfosBeforeCount].flex    = flex;
                mChildInfosBefore[mChildInfosBeforeCount].index   = count;
                mChildInfosBefore[mChildInfosBeforeCount].changed = mChildInfosBefore[mChildInfosBeforeCount].current;
                mChildInfosBeforeCount++;
            } else if (count > childIndex) {
                mChildInfosAfter[mChildInfosAfterCount].childElem = content;
                mChildInfosAfter[mChildInfosAfterCount].min     = isHorizontal ? minSize.width : minSize.height;
                mChildInfosAfter[mChildInfosAfterCount].max     = isHorizontal ? maxSize.width : maxSize.height;
                mChildInfosAfter[mChildInfosAfterCount].current = isHorizontal ? r.width : r.height;
                mChildInfosAfter[mChildInfosAfterCount].flex    = flex;
                mChildInfosAfter[mChildInfosAfterCount].index   = count;
                mChildInfosAfter[mChildInfosAfterCount].changed = mChildInfosAfter[mChildInfosAfterCount].current;
                mChildInfosAfterCount++;
            }
        } 
    }
    
    childBox = childBox->GetNextBox();
    count++;
  }

  if (!mParentBox->IsNormalDirection()) {
    // The before array is really the after array, and the order needs to be reversed.
    // First reverse both arrays.
    Reverse(mChildInfosBefore, mChildInfosBeforeCount);
    Reverse(mChildInfosAfter, mChildInfosAfterCount);

    // Now swap the two arrays.
    nscoord newAfterCount = mChildInfosBeforeCount;
    mChildInfosBeforeCount = mChildInfosAfterCount;
    mChildInfosAfterCount = newAfterCount;
    nsSplitterInfo* temp = mChildInfosAfter;
    mChildInfosAfter = mChildInfosBefore;
    mChildInfosBefore = temp;
  }

  // if the resizebefore is closest we must reverse the list because the first child in the list
  // is the Farthest we want the first child to be the closest.
  if (resizeBefore == Closest)
     Reverse(mChildInfosBefore, mChildInfosBeforeCount);

  // if the resizeafter is the Farthest we must reverse the list because the first child in the list
  // is the closest we want the first child to be the Farthest.
  if (resizeAfter == Farthest)
     Reverse(mChildInfosAfter, mChildInfosAfterCount);

  // grow only applys to the children after. If grow is set then no space should be taken out of any children after
  // us. To do this we just set the size of that list to be 0.
  if (resizeAfter == Grow)
     mChildInfosAfterCount = 0;

  nsRect vr = mOuter->GetView()->GetBounds();

  PRInt32 c;
  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(mouseEvent,
                                                               mParentBox);
  if (isHorizontal) {
     c = pt.x;
     mSplitterPos = mOuter->mRect.x;
     mSplitterViewPos = vr.x;
  } else {
     c = pt.y;
     mSplitterPos = mOuter->mRect.y;
     mSplitterViewPos = vr.y;
  }

  mDragStart = c;

  //printf("Pressed mDragStart=%d\n",mDragStart);

  mOuter->CaptureMouse(outerPresContext, PR_TRUE);

  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseMove(nsIDOMEvent* aMouseEvent)
{  
  //printf("Mouse move\n");

  if (!mPressed)
    return NS_OK;

  if (mDragging)
    return NS_OK;

  mOuter->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::state, NS_LITERAL_STRING("dragging"), PR_TRUE);

  RemoveListener();
  mDragging = PR_TRUE;

  return NS_OK;
}

void
nsSplitterFrameInner::Reverse(nsSplitterInfo*& aChildInfos, PRInt32 aCount)
{
    nsSplitterInfo* infos = new nsSplitterInfo[aCount];

    for (int i=0; i < aCount; i++)
       infos[i] = aChildInfos[aCount - 1 - i];

    delete[] aChildInfos;
    aChildInfos = infos;
}

PRBool
nsSplitterFrameInner::SupportsCollapseDirection
(
  nsSplitterFrameInner::CollapseDirection aDirection
)
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::before, &nsGkAtoms::after, &nsGkAtoms::both, nsnull};

  switch (mOuter->mContent->FindAttrValueIn(kNameSpaceID_None,
                                            nsGkAtoms::collapse,
                                            strings, eCaseMatters)) {
    case 0:
      return (aDirection == Before);
    case 1:
      return (aDirection == After);
    case 2:
      return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsSplitterFrameInner::UpdateState()
{
  // State Transitions:
  //   Open            -> Dragging
  //   Open            -> CollapsedBefore
  //   Open            -> CollapsedAfter
  //   CollapsedBefore -> Open
  //   CollapsedBefore -> Dragging
  //   CollapsedAfter  -> Open
  //   CollapsedAfter  -> Dragging
  //   Dragging        -> Open
  //   Dragging        -> CollapsedBefore (auto collapse)
  //   Dragging        -> CollapsedAfter (auto collapse)

  State newState = GetState(); 

  if (newState == mState) {
    // No change.
    return;
  }

  if (SupportsCollapseDirection(Before) || SupportsCollapseDirection(After)) {
    nsIBox* splitter = mOuter;
    // Find the splitter's immediate sibling.
    nsIBox* splitterSibling =
      nsFrameNavigator::GetChildBeforeAfter(mOuter->GetPresContext(), splitter,
                                            (newState == CollapsedBefore ||
                                             mState == CollapsedBefore));
    if (splitterSibling) {
      nsCOMPtr<nsIContent> sibling = splitterSibling->GetContent();
      if (sibling) {
        if (mState == CollapsedBefore || mState == CollapsedAfter) {
          // CollapsedBefore -> Open
          // CollapsedBefore -> Dragging
          // CollapsedAfter -> Open
          // CollapsedAfter -> Dragging
          sibling->UnsetAttr(kNameSpaceID_None, nsGkAtoms::collapsed,
                             PR_TRUE);
        } else if ((mState == Open || mState == Dragging)
                   && (newState == CollapsedBefore ||
                       newState == CollapsedAfter)) {
          // Open -> CollapsedBefore / CollapsedAfter
          // Dragging -> CollapsedBefore / CollapsedAfter
          sibling->SetAttr(kNameSpaceID_None, nsGkAtoms::collapsed,
                           NS_LITERAL_STRING("true"), PR_TRUE);
        }
      }
    }
  }
  mState = newState;
}

void
nsSplitterFrameInner::EnsureOrient()
{
  PRBool isHorizontal = !(mParentBox->GetStateBits() & NS_STATE_IS_HORIZONTAL);
  if (isHorizontal)
    mOuter->mState |= NS_STATE_IS_HORIZONTAL;
  else
    mOuter->mState &= ~NS_STATE_IS_HORIZONTAL;
}

void
nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext)
{
  EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();

  AdjustChildren(aPresContext, mChildInfosBefore, mChildInfosBeforeCount, isHorizontal);
  AdjustChildren(aPresContext, mChildInfosAfter, mChildInfosAfterCount, isHorizontal);
   
  
   // printf("----- Posting Dirty -----\n");

   
  if (realTimeDrag) {
    aPresContext->PresShell()->FlushPendingNotifications(Flush_Display);
  }
  else {
    mOuter->AddStateBits(NS_FRAME_IS_DIRTY);
    aPresContext->PresShell()->
      FrameNeedsReflow(mOuter, nsIPresShell::eTreeChange);
  }
}

static nsIBox* GetChildBoxForContent(nsIBox* aParentBox, nsIContent* aContent)
{
  nsIBox* childBox = aParentBox->GetChildBox();

  while (nsnull != childBox) {
    if (childBox->GetContent() == aContent) {
      return childBox;
    }
    childBox = childBox->GetNextBox();
  }
  return nsnull;
}

void
nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal)
{
  ///printf("------- AdjustChildren------\n");

  nsBoxLayoutState state(aPresContext);

  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  // first set all the widths.
  nsIBox* child =  mOuter->GetChildBox();
  while(child)
  {
    SetPreferredSize(state, child, onePixel, aIsHorizontal, nsnull);
    child = child->GetNextBox();
  }

  // now set our changed widths.
  for (int i=0; i < aCount; i++) 
  {
    nscoord   pref       = aChildInfos[i].changed;
    nsIBox* childBox     = GetChildBoxForContent(mParentBox, aChildInfos[i].childElem);

    if (childBox) {
      SetPreferredSize(state, childBox, onePixel, aIsHorizontal, &pref);
    }
  }
}

void
nsSplitterFrameInner::SetPreferredSize(nsBoxLayoutState& aState, nsIBox* aChildBox, nscoord aOnePixel, PRBool aIsHorizontal, nscoord* aSize)
{
  //printf("current=%d, pref=%d", current/onePixel, pref/onePixel);
 
  nscoord current = 0;

  nsRect rect(aChildBox->GetRect());
  if (aIsHorizontal) 
    current = rect.width;
  else
    current = rect.height;

  nscoord pref = 0;

  if (!aSize)
  {
    if (aIsHorizontal) 
      pref = rect.width;
    else
      pref = rect.height;
  } else {
    pref = *aSize;
  }

  nsMargin margin(0,0,0,0);
  aChildBox->GetMargin(margin);

  nsCOMPtr<nsIAtom> attribute;

  if (aIsHorizontal) {
    pref -= (margin.left + margin.right);
    attribute = nsGkAtoms::width;
  } else {
    pref -= (margin.top + margin.bottom);
    attribute = nsGkAtoms::height;
  }

  nsIContent* content = aChildBox->GetContent();

  // set its preferred size.
  nsAutoString prefValue;
  prefValue.AppendInt(pref/aOnePixel);
  if (content->AttrValueIs(kNameSpaceID_None, attribute,
                           prefValue, eCaseMatters))
     return;

  nsWeakFrame weakBox(aChildBox);
  content->SetAttr(kNameSpaceID_None, attribute, prefValue, PR_TRUE);
  ENSURE_TRUE(weakBox.IsAlive());
  aChildBox->AddStateBits(NS_FRAME_IS_DIRTY);
  aState.PresShell()->FrameNeedsReflow(aChildBox, nsIPresShell::eStyleChange);
}


void 
nsSplitterFrameInner::AddRemoveSpace(nscoord aDiff,
                                    nsSplitterInfo* aChildInfos,
                                    PRInt32 aCount,
                                    PRInt32& aSpaceLeft)
{
  aSpaceLeft = 0;

  for (int i=0; i < aCount; i++) {   
    nscoord min    = aChildInfos[i].min;
    nscoord max    = aChildInfos[i].max;
    nscoord& c     = aChildInfos[i].changed;

    // figure our how much space to add or remove
    if (c + aDiff < min) {
      aDiff += (c - min);
      c = min;
    } else if (c + aDiff > max) {
      aDiff -= (max - c);
      c = max;
    } else {
      c += aDiff;
      aDiff = 0;
    }

    // there is not space left? We are done
    if (aDiff == 0)
      break;
  }

  aSpaceLeft = aDiff;
}

/**
 * Ok if we want to resize a child we will know the actual size in pixels we want it to be.
 * This is not the preferred size. But they only way we can change a child is my manipulating its
 * preferred size. So give the actual pixel size this return method will return figure out the preferred
 * size and set it.
 */

void
nsSplitterFrameInner::ResizeChildTo(nsPresContext* aPresContext,
                                   nscoord& aDiff, 
                                   nsSplitterInfo* aChildrenBeforeInfos, 
                                   nsSplitterInfo* aChildrenAfterInfos, 
                                   PRInt32 aChildrenBeforeCount, 
                                   PRInt32 aChildrenAfterCount, 
                                   PRBool aBounded)
{ 
  nscoord spaceLeft;
  AddRemoveSpace(aDiff, aChildrenBeforeInfos,aChildrenBeforeCount,spaceLeft);

  // if there is any space left over remove it from the dif we were originally given
  aDiff -= spaceLeft;
  AddRemoveSpace(-aDiff, aChildrenAfterInfos,aChildrenAfterCount,spaceLeft);

  if (spaceLeft != 0) {
    if (aBounded) {
       aDiff += spaceLeft;
       AddRemoveSpace(spaceLeft, aChildrenBeforeInfos,aChildrenBeforeCount,spaceLeft);
    } else {
      spaceLeft = 0;
    }
  }
}


void
nsSplitterFrameInner::MoveSplitterBy(nsPresContext* aPresContext, nscoord aDiff)
{
  const nsRect& r = mOuter->mRect;
  nsIView *v = mOuter->GetView();
  nsIViewManager* vm = v->GetViewManager();
  nsRect vr = v->GetBounds();
  nsRect invalid;
  EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();
  if (isHorizontal) {
    mOuter->SetPosition(nsPoint(mSplitterPos + aDiff, r.y));
    vm->MoveViewTo(v, mSplitterViewPos + aDiff, vr.y);
    invalid.UnionRect(r,mOuter->mRect);
  } else {
    mOuter->SetPosition(nsPoint(r.x, mSplitterPos + aDiff));
    vm->MoveViewTo(v, vr.x, mSplitterViewPos + aDiff);
    invalid.UnionRect(r,mOuter->mRect);
  }

  // redraw immediately only what changed. This is animation so 
  // it must be immediate.
  nsBoxLayoutState state(aPresContext);
  mParentBox->Redraw(state, &invalid, PR_TRUE);
}
