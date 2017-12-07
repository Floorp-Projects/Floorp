/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "gfxContext.h"
#include "nsSplitterFrame.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsXULElement.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsNameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPresShell.h"
#include "nsFrameList.h"
#include "nsHTMLParts.h"
#include "nsStyleContext.h"
#include "nsBoxLayoutState.h"
#include "nsIServiceManager.h"
#include "nsContainerFrame.h"
#include "nsContentCID.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;

class nsSplitterInfo {
public:
  nscoord min;
  nscoord max;
  nscoord current;
  nscoord changed;
  nsCOMPtr<nsIContent> childElem;
  int32_t flex;
  int32_t index;
};

class nsSplitterFrameInner final : public nsIDOMEventListener
{
protected:
  virtual ~nsSplitterFrameInner();

public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit nsSplitterFrameInner(nsSplitterFrame* aSplitter)
  {
    mOuter = aSplitter;
    mPressed = false;
  }

  void Disconnect() { mOuter = nullptr; }

  nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  nsresult MouseMove(nsIDOMEvent* aMouseEvent);

  void MouseDrag(nsPresContext* aPresContext, WidgetGUIEvent* aEvent);
  void MouseUp(nsPresContext* aPresContext, WidgetGUIEvent* aEvent);

  void AdjustChildren(nsPresContext* aPresContext);
  void AdjustChildren(nsPresContext* aPresContext, nsSplitterInfo* aChildInfos, int32_t aCount, bool aIsHorizontal);

  void AddRemoveSpace(nscoord aDiff,
                    nsSplitterInfo* aChildInfos,
                    int32_t aCount,
                    int32_t& aSpaceLeft);

  void ResizeChildTo(nscoord& aDiff,
                     nsSplitterInfo* aChildrenBeforeInfos,
                     nsSplitterInfo* aChildrenAfterInfos,
                     int32_t aChildrenBeforeCount,
                     int32_t aChildrenAfterCount,
                     bool aBounded);

  void UpdateState();

  void AddListener();
  void RemoveListener();

  enum ResizeType { Closest, Farthest, Flex, Grow };
  enum State { Open, CollapsedBefore, CollapsedAfter, Dragging };
  enum CollapseDirection { Before, After };

  ResizeType GetResizeBefore();
  ResizeType GetResizeAfter();
  State GetState();

  void Reverse(UniquePtr<nsSplitterInfo[]>& aIndexes, int32_t aCount);
  bool SupportsCollapseDirection(CollapseDirection aDirection);

  void EnsureOrient();
  void SetPreferredSize(nsBoxLayoutState& aState, nsIFrame* aChildBox, nscoord aOnePixel, bool aIsHorizontal, nscoord* aSize);

  nsSplitterFrame* mOuter;
  bool mDidDrag;
  nscoord mDragStart;
  nscoord mCurrentPos;
  nsIFrame* mParentBox;
  bool mPressed;
  UniquePtr<nsSplitterInfo[]> mChildInfosBefore;
  UniquePtr<nsSplitterInfo[]> mChildInfosAfter;
  int32_t mChildInfosBeforeCount;
  int32_t mChildInfosAfterCount;
  State mState;
  nscoord mSplitterPos;
  bool mDragging;

  const Element* SplitterElement() const {
    return mOuter->GetContent()->AsElement();
  }
};

NS_IMPL_ISUPPORTS(nsSplitterFrameInner, nsIDOMEventListener)

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeBefore()
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::farthest, &nsGkAtoms::flex, nullptr};
  switch (SplitterElement()->FindAttrValueIn(kNameSpaceID_None,
                                             nsGkAtoms::resizebefore,
                                             strings, eCaseMatters)) {
    case 0: return Farthest;
    case 1: return Flex;
  }
  return Closest;
}

nsSplitterFrameInner::~nsSplitterFrameInner()
{
}

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeAfter()
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::farthest, &nsGkAtoms::flex, &nsGkAtoms::grow, nullptr};
  switch (SplitterElement()->FindAttrValueIn(kNameSpaceID_None,
                                             nsGkAtoms::resizeafter,
                                             strings, eCaseMatters)) {
    case 0: return Farthest;
    case 1: return Flex;
    case 2: return Grow;
  }
  return Closest;
}

nsSplitterFrameInner::State
nsSplitterFrameInner::GetState()
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::dragging, &nsGkAtoms::collapsed, nullptr};
  static Element::AttrValuesArray strings_substate[] =
    {&nsGkAtoms::before, &nsGkAtoms::after, nullptr};
  switch (SplitterElement()->FindAttrValueIn(kNameSpaceID_None,
                                             nsGkAtoms::state,
                                             strings, eCaseMatters)) {
    case 0: return Dragging;
    case 1:
      switch (SplitterElement()->FindAttrValueIn(kNameSpaceID_None,
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
  return new (aPresShell) nsSplitterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSplitterFrame)

nsSplitterFrame::nsSplitterFrame(nsStyleContext* aContext)
: nsBoxFrame(aContext, kClassID),
  mInner(0)
{
}

void
nsSplitterFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  if (mInner) {
    mInner->RemoveListener();
    mInner->Disconnect();
    mInner->Release();
    mInner = nullptr;
  }
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}


nsresult
nsSplitterFrame::GetCursor(const nsPoint&    aPoint,
                           nsIFrame::Cursor& aCursor)
{
  return nsBoxFrame::GetCursor(aPoint, aCursor);

  /*
    if (IsXULHorizontal())
      aCursor = NS_STYLE_CURSOR_N_RESIZE;
    else
      aCursor = NS_STYLE_CURSOR_W_RESIZE;

    return NS_OK;
  */
}

nsresult
nsSplitterFrame::AttributeChanged(int32_t aNameSpaceID,
                                  nsAtom* aAttribute,
                                  int32_t aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  // if the alignment changed. Let the grippy know
  if (aAttribute == nsGkAtoms::align) {
    // tell the slider its attribute changed so it can
    // update itself
    nsIFrame* grippy = nullptr;
    nsScrollbarButtonFrame::GetChildWithTag(nsGkAtoms::grippy, this, grippy);
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
void
nsSplitterFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  MOZ_ASSERT(!mInner);
  mInner = new nsSplitterFrameInner(this);

  mInner->AddRef();
  mInner->mState = nsSplitterFrameInner::Open;
  mInner->mDragging = false;

  // determine orientation of parent, and if vertical, set orient to vertical
  // on splitter content, then re-resolve style
  // XXXbz this is pretty messed up, since this can change whether we should
  // have a frame at all.  This really needs a better solution.
  if (aParent && aParent->IsXULBoxFrame()) {
    if (!aParent->IsXULHorizontal()) {
      if (!nsContentUtils::HasNonEmptyAttr(aContent, kNameSpaceID_None,
                                           nsGkAtoms::orient)) {
        aContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                       NS_LITERAL_STRING("vertical"), false);
        if (StyleContext()->IsGecko()) {
          // FIXME(emilio): Even if we did this in Servo, this just won't
          // work, and we'd need a specific "really re-resolve the style" API...
          GeckoStyleContext* parentStyleContext =
            StyleContext()->AsGecko()->GetParent();
          RefPtr<nsStyleContext> newContext = PresContext()->StyleSet()->
            ResolveStyleFor(aContent->AsElement(), parentStyleContext,
                            LazyComputeBehavior::Allow);
          SetStyleContextWithoutNotification(newContext);
        }
      }
    }
  }

  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mInner->mState = nsSplitterFrameInner::Open;
  mInner->AddListener();
  mInner->mParentBox = nullptr;
}

NS_IMETHODIMP
nsSplitterFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW)
  {
    mInner->mParentBox = nsBox::GetParentXULBox(this);
    mInner->UpdateState();
  }

  return nsBoxFrame::DoXULLayout(aState);
}


void
nsSplitterFrame::GetInitialOrientation(bool& aIsHorizontal)
{
  nsIFrame* box = nsBox::GetParentXULBox(this);
  if (box) {
    aIsHorizontal = !box->IsXULHorizontal();
  }
  else
    nsBoxFrame::GetInitialOrientation(aIsHorizontal);
}

NS_IMETHODIMP
nsSplitterFrame::HandlePress(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleMultiplePress(nsPresContext* aPresContext,
                                     WidgetGUIEvent* aEvent,
                                     nsEventStatus* aEventStatus,
                                     bool aControlHeld)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleDrag(nsPresContext* aPresContext,
                            WidgetGUIEvent* aEvent,
                            nsEventStatus* aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleRelease(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus)
{
  return NS_OK;
}

void
nsSplitterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsDisplayListSet& aLists)
{
  nsBoxFrame::BuildDisplayList(aBuilder, aLists);

  // if the mouse is captured always return us as the frame.
  if (mInner->mDragging && aBuilder->IsForEventDelivery())
  {
    // XXX It's probably better not to check visibility here, right?
    aLists.Outlines()->AppendToTop(new (aBuilder)
      nsDisplayEventReceiver(aBuilder, this));
    return;
  }
}

nsresult
nsSplitterFrame::HandleEvent(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  AutoWeakFrame weakFrame(this);
  RefPtr<nsSplitterFrameInner> inner(mInner);
  switch (aEvent->mMessage) {
    case eMouseMove:
      inner->MouseDrag(aPresContext, aEvent);
      break;

    case eMouseUp:
      if (aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
        inner->MouseUp(aPresContext, aEvent);
      }
      break;

    default:
      break;
  }

  NS_ENSURE_STATE(weakFrame.IsAlive());
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void
nsSplitterFrameInner::MouseUp(nsPresContext* aPresContext,
                              WidgetGUIEvent* aEvent)
{
  if (mDragging && mOuter) {
    AdjustChildren(aPresContext);
    AddListener();
    nsIPresShell::SetCapturingContent(nullptr, 0); // XXXndeakin is this needed?
    mDragging = false;
    State newState = GetState();
    // if the state is dragging then make it Open.
    if (newState == Dragging) {
      mOuter->mContent->AsElement()->SetAttr(kNameSpaceID_None,
                                             nsGkAtoms::state, EmptyString(),
                                             true);
    }

    mPressed = false;

    // if we dragged then fire a command event.
    if (mDidDrag) {
      RefPtr<nsXULElement> element =
        nsXULElement::FromContent(mOuter->GetContent());
      element->DoCommand();
    }

    //printf("MouseUp\n");
  }

  mChildInfosBefore = nullptr;
  mChildInfosAfter = nullptr;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;
}

void
nsSplitterFrameInner::MouseDrag(nsPresContext* aPresContext,
                                WidgetGUIEvent* aEvent)
{
  if (mDragging && mOuter) {

    //printf("Dragging\n");

    bool isHorizontal = !mOuter->IsXULHorizontal();
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

    bool bounded;

    if (resizeAfter == nsSplitterFrameInner::Grow)
      bounded = false;
    else
      bounded = true;

    int i;
    for (i=0; i < mChildInfosBeforeCount; i++)
      mChildInfosBefore[i].changed = mChildInfosBefore[i].current;

    for (i=0; i < mChildInfosAfterCount; i++)
      mChildInfosAfter[i].changed = mChildInfosAfter[i].current;

    nscoord oldPos = pos;

    ResizeChildTo(pos,
                  mChildInfosBefore.get(), mChildInfosAfter.get(),
                  mChildInfosBeforeCount, mChildInfosAfterCount, bounded);

    State currentState = GetState();
    bool supportsBefore = SupportsCollapseDirection(Before);
    bool supportsAfter = SupportsCollapseDirection(After);

    const bool isRTL = mOuter->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
    bool pastEnd = oldPos > 0 && oldPos > pos;
    bool pastBegin = oldPos < 0 && oldPos < pos;
    if (isRTL) {
      // Swap the boundary checks in RTL mode
      bool tmp = pastEnd;
      pastEnd = pastBegin;
      pastBegin = tmp;
    }
    const bool isCollapsedBefore = pastBegin && supportsBefore;
    const bool isCollapsedAfter = pastEnd && supportsAfter;

    // if we are in a collapsed position
    if (isCollapsedBefore || isCollapsedAfter)
    {
      // and we are not collapsed then collapse
      if (currentState == Dragging) {
        if (pastEnd)
        {
          //printf("Collapse right\n");
          if (supportsAfter)
          {
            RefPtr<Element> outer = mOuter->mContent->AsElement();
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate,
                           NS_LITERAL_STRING("after"),
                           true);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                           NS_LITERAL_STRING("collapsed"),
                           true);
          }

        } else if (pastBegin)
        {
          //printf("Collapse left\n");
          if (supportsBefore)
          {
            RefPtr<Element> outer = mOuter->mContent->AsElement();
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate,
                           NS_LITERAL_STRING("before"),
                           true);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                           NS_LITERAL_STRING("collapsed"),
                           true);
          }
        }
      }
    } else {
      // if we are not in a collapsed position and we are not dragging make sure
      // we are dragging.
      if (currentState != Dragging) {
        mOuter->mContent->AsElement()->SetAttr(kNameSpaceID_None,
                                               nsGkAtoms::state,
                                               NS_LITERAL_STRING("dragging"),
                                               true);
      }
      AdjustChildren(aPresContext);
    }

    mDidDrag = true;
  }
}

void
nsSplitterFrameInner::AddListener()
{
  mOuter->GetContent()->
    AddEventListener(NS_LITERAL_STRING("mouseup"), this, false, false);
  mOuter->GetContent()->
    AddEventListener(NS_LITERAL_STRING("mousedown"), this, false, false);
  mOuter->GetContent()->
    AddEventListener(NS_LITERAL_STRING("mousemove"), this, false, false);
  mOuter->GetContent()->
    AddEventListener(NS_LITERAL_STRING("mouseout"), this, false, false);
}

void
nsSplitterFrameInner::RemoveListener()
{
  ENSURE_TRUE(mOuter);
  mOuter->GetContent()->
    RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, false);
  mOuter->GetContent()->
    RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, false);
  mOuter->GetContent()->
    RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, false);
  mOuter->GetContent()->
    RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, false);
}

nsresult
nsSplitterFrameInner::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mouseup"))
    return MouseUp(aEvent);
  if (eventType.EqualsLiteral("mousedown"))
    return MouseDown(aEvent);
  if (eventType.EqualsLiteral("mousemove") ||
      eventType.EqualsLiteral("mouseout"))
    return MouseMove(aEvent);

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseUp(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mOuter, NS_OK);
  mPressed = false;

  nsIPresShell::SetCapturingContent(nullptr, 0);

  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mOuter, NS_OK);
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
  if (!mouseEvent)
    return NS_OK;

  int16_t button = 0;
  mouseEvent->GetButton(&button);

  // only if left button
  if (button != 0)
     return NS_OK;

  if (SplitterElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                     nsGkAtoms::_true, eCaseMatters))
    return NS_OK;

  mParentBox = nsBox::GetParentXULBox(mOuter);
  if (!mParentBox)
    return NS_OK;

  // get our index
  nsPresContext* outerPresContext = mOuter->PresContext();
  const nsFrameList& siblingList(mParentBox->PrincipalChildList());
  int32_t childIndex = siblingList.IndexOf(mOuter);
  // if it's 0 (or not found) then stop right here.
  // It might be not found if we're not in the parent's primary frame list.
  if (childIndex <= 0)
    return NS_OK;

  int32_t childCount = siblingList.GetLength();
  // if it's the last index then we need to allow for resizeafter="grow"
  if (childIndex == childCount - 1 && GetResizeAfter() != Grow)
    return NS_OK;

  RefPtr<gfxContext> rc =
    outerPresContext->PresShell()->CreateReferenceRenderingContext();
  nsBoxLayoutState state(outerPresContext, rc);
  mCurrentPos = 0;
  mPressed = true;

  mDidDrag = false;

  EnsureOrient();
  bool isHorizontal = !mOuter->IsXULHorizontal();

  ResizeType resizeBefore = GetResizeBefore();
  ResizeType resizeAfter  = GetResizeAfter();

  mChildInfosBefore = MakeUnique<nsSplitterInfo[]>(childCount);
  mChildInfosAfter  = MakeUnique<nsSplitterInfo[]>(childCount);

  // create info 2 lists. One of the children before us and one after.
  int32_t count = 0;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;

  nsIFrame* childBox = nsBox::GetChildXULBox(mParentBox);

  while (nullptr != childBox)
  {
    nsIContent* content = childBox->GetContent();
    nsIDocument* doc = content->OwnerDoc();
    int32_t dummy;
    nsAtom* atom = doc->BindingManager()->ResolveTag(content, &dummy);

    // skip over any splitters
    if (atom != nsGkAtoms::splitter) {
        nsSize prefSize = childBox->GetXULPrefSize(state);
        nsSize minSize = childBox->GetXULMinSize(state);
        nsSize maxSize = nsBox::BoundsCheckMinMax(minSize, childBox->GetXULMaxSize(state));
        prefSize = nsBox::BoundsCheck(minSize, prefSize, maxSize);

        mOuter->AddMargin(childBox, minSize);
        mOuter->AddMargin(childBox, prefSize);
        mOuter->AddMargin(childBox, maxSize);

        nscoord flex = childBox->GetXULFlex();

        nsMargin margin(0,0,0,0);
        childBox->GetXULMargin(margin);
        nsRect r(childBox->GetRect());
        r.Inflate(margin);

        // We need to check for hidden attribute too, since treecols with
        // the hidden="true" attribute are not really hidden, just collapsed
        if (!content->IsElement() ||
            (!content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                nsGkAtoms::fixed,
                                                nsGkAtoms::_true,
                                                eCaseMatters) &&
             !content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                nsGkAtoms::hidden,
                                                nsGkAtoms::_true,
                                                eCaseMatters))) {
            if (count < childIndex && (resizeBefore != Flex || flex > 0)) {
                mChildInfosBefore[mChildInfosBeforeCount].childElem = content;
                mChildInfosBefore[mChildInfosBeforeCount].min     = isHorizontal ? minSize.width : minSize.height;
                mChildInfosBefore[mChildInfosBeforeCount].max     = isHorizontal ? maxSize.width : maxSize.height;
                mChildInfosBefore[mChildInfosBeforeCount].current = isHorizontal ? r.width : r.height;
                mChildInfosBefore[mChildInfosBeforeCount].flex    = flex;
                mChildInfosBefore[mChildInfosBeforeCount].index   = count;
                mChildInfosBefore[mChildInfosBeforeCount].changed = mChildInfosBefore[mChildInfosBeforeCount].current;
                mChildInfosBeforeCount++;
            } else if (count > childIndex && (resizeAfter != Flex || flex > 0)) {
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

    childBox = nsBox::GetNextXULBox(childBox);
    count++;
  }

  if (!mParentBox->IsXULNormalDirection()) {
    // The before array is really the after array, and the order needs to be reversed.
    // First reverse both arrays.
    Reverse(mChildInfosBefore, mChildInfosBeforeCount);
    Reverse(mChildInfosAfter, mChildInfosAfterCount);

    // Now swap the two arrays.
    Swap(mChildInfosBeforeCount, mChildInfosAfterCount);
    Swap(mChildInfosBefore, mChildInfosAfter);
  }

  // if resizebefore is not Farthest, reverse the list because the first child
  // in the list is the farthest, and we want the first child to be the closest.
  if (resizeBefore != Farthest)
     Reverse(mChildInfosBefore, mChildInfosBeforeCount);

  // if the resizeafter is the Farthest we must reverse the list because the first child in the list
  // is the closest we want the first child to be the Farthest.
  if (resizeAfter == Farthest)
     Reverse(mChildInfosAfter, mChildInfosAfterCount);

  // grow only applys to the children after. If grow is set then no space should be taken out of any children after
  // us. To do this we just set the size of that list to be 0.
  if (resizeAfter == Grow)
     mChildInfosAfterCount = 0;

  int32_t c;
  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(mouseEvent->AsEvent(),
                                                               mParentBox);
  if (isHorizontal) {
     c = pt.x;
     mSplitterPos = mOuter->mRect.x;
  } else {
     c = pt.y;
     mSplitterPos = mOuter->mRect.y;
  }

  mDragStart = c;

  //printf("Pressed mDragStart=%d\n",mDragStart);

  nsIPresShell::SetCapturingContent(mOuter->GetContent(), CAPTURE_IGNOREALLOWED);

  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseMove(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mOuter, NS_OK);
  if (!mPressed)
    return NS_OK;

  if (mDragging)
    return NS_OK;

  nsCOMPtr<nsIDOMEventListener> kungfuDeathGrip(this);
  mOuter->mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                                         NS_LITERAL_STRING("dragging"), true);

  RemoveListener();
  mDragging = true;

  return NS_OK;
}

void
nsSplitterFrameInner::Reverse(UniquePtr<nsSplitterInfo[]>& aChildInfos, int32_t aCount)
{
    UniquePtr<nsSplitterInfo[]> infos(new nsSplitterInfo[aCount]);

    for (int i=0; i < aCount; i++)
       infos[i] = aChildInfos[aCount - 1 - i];

    aChildInfos = Move(infos);
}

bool
nsSplitterFrameInner::SupportsCollapseDirection
(
  nsSplitterFrameInner::CollapseDirection aDirection
)
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::before, &nsGkAtoms::after, &nsGkAtoms::both, nullptr};

  switch (SplitterElement()->FindAttrValueIn(kNameSpaceID_None,
                                             nsGkAtoms::collapse,
                                             strings, eCaseMatters)) {
    case 0:
      return (aDirection == Before);
    case 1:
      return (aDirection == After);
    case 2:
      return true;
  }

  return false;
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

  if ((SupportsCollapseDirection(Before) || SupportsCollapseDirection(After)) &&
      mOuter->GetParent()->IsXULBoxFrame()) {
    // Find the splitter's immediate sibling.
    nsIFrame* splitterSibling;
    if (newState == CollapsedBefore || mState == CollapsedBefore) {
      splitterSibling = mOuter->GetPrevSibling();
    } else {
      splitterSibling = mOuter->GetNextSibling();
    }

    if (splitterSibling) {
      nsCOMPtr<nsIContent> sibling = splitterSibling->GetContent();
      if (sibling && sibling->IsElement()) {
        if (mState == CollapsedBefore || mState == CollapsedAfter) {
          // CollapsedBefore -> Open
          // CollapsedBefore -> Dragging
          // CollapsedAfter -> Open
          // CollapsedAfter -> Dragging
          nsContentUtils::AddScriptRunner(
            new nsUnsetAttrRunnable(sibling->AsElement(), nsGkAtoms::collapsed));
        } else if ((mState == Open || mState == Dragging)
                   && (newState == CollapsedBefore ||
                       newState == CollapsedAfter)) {
          // Open -> CollapsedBefore / CollapsedAfter
          // Dragging -> CollapsedBefore / CollapsedAfter
          nsContentUtils::AddScriptRunner(
            new nsSetAttrRunnable(sibling->AsElement(), nsGkAtoms::collapsed,
                                  NS_LITERAL_STRING("true")));
        }
      }
    }
  }
  mState = newState;
}

void
nsSplitterFrameInner::EnsureOrient()
{
  bool isHorizontal = !(mParentBox->GetStateBits() & NS_STATE_IS_HORIZONTAL);
  if (isHorizontal)
    mOuter->AddStateBits(NS_STATE_IS_HORIZONTAL);
  else
    mOuter->RemoveStateBits(NS_STATE_IS_HORIZONTAL);
}

void
nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext)
{
  EnsureOrient();
  bool isHorizontal = !mOuter->IsXULHorizontal();

  AdjustChildren(aPresContext, mChildInfosBefore.get(),
                 mChildInfosBeforeCount, isHorizontal);
  AdjustChildren(aPresContext, mChildInfosAfter.get(),
                 mChildInfosAfterCount, isHorizontal);
}

static nsIFrame* GetChildBoxForContent(nsIFrame* aParentBox, nsIContent* aContent)
{
  nsIFrame* childBox = nsBox::GetChildXULBox(aParentBox);

  while (nullptr != childBox) {
    if (childBox->GetContent() == aContent) {
      return childBox;
    }
    childBox = nsBox::GetNextXULBox(childBox);
  }
  return nullptr;
}

void
nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext, nsSplitterInfo* aChildInfos, int32_t aCount, bool aIsHorizontal)
{
  ///printf("------- AdjustChildren------\n");

  nsBoxLayoutState state(aPresContext);

  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  // first set all the widths.
  nsIFrame* child =  nsBox::GetChildXULBox(mOuter);
  while(child)
  {
    SetPreferredSize(state, child, onePixel, aIsHorizontal, nullptr);
    child = nsBox::GetNextXULBox(child);
  }

  // now set our changed widths.
  for (int i=0; i < aCount; i++)
  {
    nscoord   pref       = aChildInfos[i].changed;
    nsIFrame* childBox     = GetChildBoxForContent(mParentBox, aChildInfos[i].childElem);

    if (childBox) {
      SetPreferredSize(state, childBox, onePixel, aIsHorizontal, &pref);
    }
  }
}

void
nsSplitterFrameInner::SetPreferredSize(nsBoxLayoutState& aState, nsIFrame* aChildBox, nscoord aOnePixel, bool aIsHorizontal, nscoord* aSize)
{
  nsRect rect(aChildBox->GetRect());
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
  aChildBox->GetXULMargin(margin);

  RefPtr<nsAtom> attribute;

  if (aIsHorizontal) {
    pref -= (margin.left + margin.right);
    attribute = nsGkAtoms::width;
  } else {
    pref -= (margin.top + margin.bottom);
    attribute = nsGkAtoms::height;
  }

  nsIContent* content = aChildBox->GetContent();
  if (!content->IsElement()) {
    return;
  }

  // set its preferred size.
  nsAutoString prefValue;
  prefValue.AppendInt(pref/aOnePixel);
  if (content->AsElement()->AttrValueIs(kNameSpaceID_None, attribute,
                                        prefValue, eCaseMatters)) {
     return;
  }

  AutoWeakFrame weakBox(aChildBox);
  content->AsElement()->SetAttr(kNameSpaceID_None, attribute, prefValue, true);
  ENSURE_TRUE(weakBox.IsAlive());
  aState.PresShell()->FrameNeedsReflow(aChildBox, nsIPresShell::eStyleChange,
                                       NS_FRAME_IS_DIRTY);
}


void
nsSplitterFrameInner::AddRemoveSpace(nscoord aDiff,
                                    nsSplitterInfo* aChildInfos,
                                    int32_t aCount,
                                    int32_t& aSpaceLeft)
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
nsSplitterFrameInner::ResizeChildTo(nscoord& aDiff,
                                    nsSplitterInfo* aChildrenBeforeInfos,
                                    nsSplitterInfo* aChildrenAfterInfos,
                                    int32_t aChildrenBeforeCount,
                                    int32_t aChildrenAfterCount,
                                    bool aBounded)
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
    }
  }
}
