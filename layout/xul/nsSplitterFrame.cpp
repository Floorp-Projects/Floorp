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
#include "nsXULElement.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"
#include "nsNameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIDOMEventListener.h"
#include "nsICSSDeclaration.h"
#include "nsFrameList.h"
#include "nsHTMLParts.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/CSSOrderAwareFrameIterator.h"
#include "nsBoxLayoutState.h"
#include "nsContainerFrame.h"
#include "nsContentCID.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/UniquePtr.h"
#include "nsStyledElement.h"

using namespace mozilla;

using mozilla::dom::Element;
using mozilla::dom::Event;

class nsSplitterInfo {
 public:
  nscoord min;
  nscoord max;
  nscoord current;
  nscoord changed;
  nsCOMPtr<nsIContent> childElem;
  int32_t flex;
};

class nsSplitterFrameInner final : public nsIDOMEventListener {
 protected:
  virtual ~nsSplitterFrameInner();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit nsSplitterFrameInner(nsSplitterFrame* aSplitter)
      : mOuter(aSplitter) {}

  void Disconnect() { mOuter = nullptr; }

  nsresult MouseDown(Event* aMouseEvent);
  nsresult MouseUp(Event* aMouseEvent);
  nsresult MouseMove(Event* aMouseEvent);

  void MouseDrag(nsPresContext* aPresContext, WidgetGUIEvent* aEvent);
  void MouseUp(nsPresContext* aPresContext, WidgetGUIEvent* aEvent);

  void AdjustChildren(nsPresContext* aPresContext);
  void AdjustChildren(nsPresContext* aPresContext, nsSplitterInfo* aChildInfos,
                      int32_t aCount, bool aIsHorizontal);

  void AddRemoveSpace(nscoord aDiff, nsSplitterInfo* aChildInfos,
                      int32_t aCount, int32_t& aSpaceLeft);

  void ResizeChildTo(nscoord& aDiff, nsSplitterInfo* aChildrenBeforeInfos,
                     nsSplitterInfo* aChildrenAfterInfos,
                     int32_t aChildrenBeforeCount, int32_t aChildrenAfterCount,
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
  void SetPreferredSize(nsBoxLayoutState& aState, nsIFrame* aChildBox,
                        bool aIsHorizontal, nscoord* aSize);

  nsSplitterFrame* mOuter;
  bool mDidDrag = false;
  nscoord mDragStart = 0;
  nsIFrame* mParentBox = nullptr;
  bool mPressed = false;
  UniquePtr<nsSplitterInfo[]> mChildInfosBefore;
  UniquePtr<nsSplitterInfo[]> mChildInfosAfter;
  int32_t mChildInfosBeforeCount = 0;
  int32_t mChildInfosAfterCount = 0;
  State mState = Open;
  nscoord mSplitterPos = 0;
  bool mDragging = false;

  const Element* SplitterElement() const {
    return mOuter->GetContent()->AsElement();
  }
};

NS_IMPL_ISUPPORTS(nsSplitterFrameInner, nsIDOMEventListener)

nsSplitterFrameInner::ResizeType nsSplitterFrameInner::GetResizeBefore() {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::farthest,
                                               nsGkAtoms::flex, nullptr};
  switch (SplitterElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::resizebefore, strings, eCaseMatters)) {
    case 0:
      return Farthest;
    case 1:
      return Flex;
  }
  return Closest;
}

nsSplitterFrameInner::~nsSplitterFrameInner() = default;

nsSplitterFrameInner::ResizeType nsSplitterFrameInner::GetResizeAfter() {
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::farthest, nsGkAtoms::flex, nsGkAtoms::grow, nullptr};
  switch (SplitterElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::resizeafter, strings, eCaseMatters)) {
    case 0:
      return Farthest;
    case 1:
      return Flex;
    case 2:
      return Grow;
  }
  return Closest;
}

nsSplitterFrameInner::State nsSplitterFrameInner::GetState() {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::dragging,
                                               nsGkAtoms::collapsed, nullptr};
  static Element::AttrValuesArray strings_substate[] = {
      nsGkAtoms::before, nsGkAtoms::after, nullptr};
  switch (SplitterElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::state, strings, eCaseMatters)) {
    case 0:
      return Dragging;
    case 1:
      switch (SplitterElement()->FindAttrValueIn(
          kNameSpaceID_None, nsGkAtoms::substate, strings_substate,
          eCaseMatters)) {
        case 0:
          return CollapsedBefore;
        case 1:
          return CollapsedAfter;
        default:
          if (SupportsCollapseDirection(After)) return CollapsedAfter;
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
nsIFrame* NS_NewSplitterFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsSplitterFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSplitterFrame)

nsSplitterFrame::nsSplitterFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID), mInner(0) {}

void nsSplitterFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                  PostDestroyData& aPostDestroyData) {
  if (mInner) {
    mInner->RemoveListener();
    mInner->Disconnect();
    mInner->Release();
    mInner = nullptr;
  }
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult nsSplitterFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  nsresult rv =
      nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  if (aAttribute == nsGkAtoms::state) {
    mInner->UpdateState();
  }

  return rv;
}

/**
 * Initialize us. If we are in a box get our alignment so we know what direction
 * we are
 */
void nsSplitterFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  MOZ_ASSERT(!mInner);
  mInner = new nsSplitterFrameInner(this);

  mInner->AddRef();

  // determine orientation of parent, and if vertical, set orient to vertical
  // on splitter content, then re-resolve style
  // XXXbz this is pretty messed up, since this can change whether we should
  // have a frame at all.  This really needs a better solution.
  if (aParent && aParent->IsXULBoxFrame()) {
    if (!aParent->IsXULHorizontal()) {
      if (!nsContentUtils::HasNonEmptyAttr(aContent, kNameSpaceID_None,
                                           nsGkAtoms::orient)) {
        aContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                       u"vertical"_ns, false);
      }
    }
  }

  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mInner->mState = nsSplitterFrameInner::Open;
  mInner->AddListener();
  mInner->mParentBox = nullptr;
}

NS_IMETHODIMP
nsSplitterFrame::DoXULLayout(nsBoxLayoutState& aState) {
  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    mInner->mParentBox = nsIFrame::GetParentXULBox(this);
    mInner->UpdateState();
  }

  return nsBoxFrame::DoXULLayout(aState);
}

void nsSplitterFrame::GetInitialOrientation(bool& aIsHorizontal) {
  nsIFrame* box = nsIFrame::GetParentXULBox(this);
  if (box) {
    aIsHorizontal = !box->IsXULHorizontal();
  } else
    nsBoxFrame::GetInitialOrientation(aIsHorizontal);
}

NS_IMETHODIMP
nsSplitterFrame::HandlePress(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus) {
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleMultiplePress(nsPresContext* aPresContext,
                                     WidgetGUIEvent* aEvent,
                                     nsEventStatus* aEventStatus,
                                     bool aControlHeld) {
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleDrag(nsPresContext* aPresContext, WidgetGUIEvent* aEvent,
                            nsEventStatus* aEventStatus) {
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleRelease(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) {
  return NS_OK;
}

void nsSplitterFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  nsBoxFrame::BuildDisplayList(aBuilder, aLists);

  // if the mouse is captured always return us as the frame.
  if (mInner->mDragging && aBuilder->IsForEventDelivery()) {
    // XXX It's probably better not to check visibility here, right?
    aLists.Outlines()->AppendNewToTop<nsDisplayEventReceiver>(aBuilder, this);
    return;
  }
}

nsresult nsSplitterFrame::HandleEvent(nsPresContext* aPresContext,
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus) {
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
      if (aEvent->AsMouseEvent()->mButton == MouseButton::ePrimary) {
        inner->MouseUp(aPresContext, aEvent);
      }
      break;

    default:
      break;
  }

  NS_ENSURE_STATE(weakFrame.IsAlive());
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void nsSplitterFrameInner::MouseUp(nsPresContext* aPresContext,
                                   WidgetGUIEvent* aEvent) {
  if (mDragging && mOuter) {
    AdjustChildren(aPresContext);
    AddListener();
    PresShell::ReleaseCapturingContent();  // XXXndeakin is this needed?
    mDragging = false;
    State newState = GetState();
    // if the state is dragging then make it Open.
    if (newState == Dragging) {
      mOuter->mContent->AsElement()->SetAttr(kNameSpaceID_None,
                                             nsGkAtoms::state, u""_ns, true);
    }

    mPressed = false;

    // if we dragged then fire a command event.
    if (mDidDrag) {
      RefPtr<nsXULElement> element =
          nsXULElement::FromNode(mOuter->GetContent());
      element->DoCommand();
    }

    // printf("MouseUp\n");
  }

  mChildInfosBefore = nullptr;
  mChildInfosAfter = nullptr;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;
}

void nsSplitterFrameInner::MouseDrag(nsPresContext* aPresContext,
                                     WidgetGUIEvent* aEvent) {
  if (mDragging && mOuter) {
    // printf("Dragging\n");

    bool isHorizontal = !mOuter->IsXULHorizontal();
    // convert coord to pixels
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(
        aEvent, RelativeTo{mParentBox});
    nscoord pos = isHorizontal ? pt.x : pt.y;

    // mDragStart is in frame coordinates
    nscoord start = mDragStart;

    // take our current position and subtract the start location
    pos -= start;

    // printf("Diff=%d\n", pos);

    ResizeType resizeAfter = GetResizeAfter();

    const bool bounded = resizeAfter != nsSplitterFrameInner::Grow;

    int i;
    for (i = 0; i < mChildInfosBeforeCount; i++)
      mChildInfosBefore[i].changed = mChildInfosBefore[i].current;

    for (i = 0; i < mChildInfosAfterCount; i++)
      mChildInfosAfter[i].changed = mChildInfosAfter[i].current;

    nscoord oldPos = pos;

    ResizeChildTo(pos, mChildInfosBefore.get(), mChildInfosAfter.get(),
                  mChildInfosBeforeCount, mChildInfosAfterCount, bounded);

    State currentState = GetState();
    bool supportsBefore = SupportsCollapseDirection(Before);
    bool supportsAfter = SupportsCollapseDirection(After);

    const bool isRTL =
        mOuter->StyleVisibility()->mDirection == StyleDirection::Rtl;
    bool pastEnd = oldPos > 0 && oldPos > pos;
    bool pastBegin = oldPos < 0 && oldPos < pos;
    if (isRTL) {
      // Swap the boundary checks in RTL mode
      std::swap(pastEnd, pastBegin);
    }
    const bool isCollapsedBefore = pastBegin && supportsBefore;
    const bool isCollapsedAfter = pastEnd && supportsAfter;

    // if we are in a collapsed position
    if (isCollapsedBefore || isCollapsedAfter) {
      // and we are not collapsed then collapse
      if (currentState == Dragging) {
        if (pastEnd) {
          // printf("Collapse right\n");
          if (supportsAfter) {
            RefPtr<Element> outer = mOuter->mContent->AsElement();
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate, u"after"_ns,
                           true);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state, u"collapsed"_ns,
                           true);
          }

        } else if (pastBegin) {
          // printf("Collapse left\n");
          if (supportsBefore) {
            RefPtr<Element> outer = mOuter->mContent->AsElement();
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::substate, u"before"_ns,
                           true);
            outer->SetAttr(kNameSpaceID_None, nsGkAtoms::state, u"collapsed"_ns,
                           true);
          }
        }
      }
    } else {
      // if we are not in a collapsed position and we are not dragging make sure
      // we are dragging.
      if (currentState != Dragging) {
        mOuter->mContent->AsElement()->SetAttr(
            kNameSpaceID_None, nsGkAtoms::state, u"dragging"_ns, true);
      }
      AdjustChildren(aPresContext);
    }

    mDidDrag = true;
  }
}

void nsSplitterFrameInner::AddListener() {
  mOuter->GetContent()->AddEventListener(u"mouseup"_ns, this, false, false);
  mOuter->GetContent()->AddEventListener(u"mousedown"_ns, this, false, false);
  mOuter->GetContent()->AddEventListener(u"mousemove"_ns, this, false, false);
  mOuter->GetContent()->AddEventListener(u"mouseout"_ns, this, false, false);
}

void nsSplitterFrameInner::RemoveListener() {
  NS_ENSURE_TRUE_VOID(mOuter);
  mOuter->GetContent()->RemoveEventListener(u"mouseup"_ns, this, false);
  mOuter->GetContent()->RemoveEventListener(u"mousedown"_ns, this, false);
  mOuter->GetContent()->RemoveEventListener(u"mousemove"_ns, this, false);
  mOuter->GetContent()->RemoveEventListener(u"mouseout"_ns, this, false);
}

nsresult nsSplitterFrameInner::HandleEvent(dom::Event* aEvent) {
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mouseup")) return MouseUp(aEvent);
  if (eventType.EqualsLiteral("mousedown")) return MouseDown(aEvent);
  if (eventType.EqualsLiteral("mousemove") ||
      eventType.EqualsLiteral("mouseout"))
    return MouseMove(aEvent);

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}

nsresult nsSplitterFrameInner::MouseUp(Event* aMouseEvent) {
  NS_ENSURE_TRUE(mOuter, NS_OK);
  mPressed = false;

  PresShell::ReleaseCapturingContent();

  return NS_OK;
}

nsresult nsSplitterFrameInner::MouseDown(Event* aMouseEvent) {
  NS_ENSURE_TRUE(mOuter, NS_OK);
  dom::MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  if (!mouseEvent) {
    return NS_OK;
  }

  // only if left button
  if (mouseEvent->Button() != 0) {
    return NS_OK;
  }

  if (SplitterElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                     nsGkAtoms::_true, eCaseMatters))
    return NS_OK;

  mParentBox = nsIFrame::GetParentXULBox(mOuter);
  if (!mParentBox) {
    return NS_OK;
  }

  // get our index
  nsPresContext* outerPresContext = mOuter->PresContext();

  const int32_t childCount = mParentBox->PrincipalChildList().GetLength();
  RefPtr<gfxContext> rc =
      outerPresContext->PresShell()->CreateReferenceRenderingContext();
  nsBoxLayoutState state(outerPresContext, rc);

  mDidDrag = false;

  EnsureOrient();
  bool isHorizontal = !mOuter->IsXULHorizontal();

  ResizeType resizeBefore = GetResizeBefore();
  ResizeType resizeAfter = GetResizeAfter();

  mChildInfosBefore = MakeUnique<nsSplitterInfo[]>(childCount);
  mChildInfosAfter = MakeUnique<nsSplitterInfo[]>(childCount);

  // create info 2 lists. One of the children before us and one after.
  int32_t count = 0;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;

  bool foundOuter = false;
  CSSOrderAwareFrameIterator iter(
      mParentBox, layout::kPrincipalList,
      CSSOrderAwareFrameIterator::ChildFilter::IncludeAll,
      CSSOrderAwareFrameIterator::OrderState::Unknown,
      CSSOrderAwareFrameIterator::OrderingProperty::BoxOrdinalGroup);
  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* childBox = iter.get();
    if (childBox == mOuter) {
      foundOuter = true;
      if (!count) {
        // We're at the beginning, nothing to do.
        return NS_OK;
      }
      if (count == childCount - 1 && resizeAfter != Grow) {
        // if it's the last index then we need to allow for resizeafter="grow"
        return NS_OK;
      }
    }
    count++;

    nsIContent* content = childBox->GetContent();

    if (auto* element = nsXULElement::FromNode(content)) {
      if (element->NodeInfo()->NameAtom() == nsGkAtoms::splitter) {
        // skip over any splitters
        continue;
      }

      // We need to check for hidden attribute too, since treecols with
      // the hidden="true" attribute are not really hidden, just collapsed
      if (element->GetXULBoolAttr(nsGkAtoms::fixed) ||
          element->GetXULBoolAttr(nsGkAtoms::hidden)) {
        continue;
      }
    }

    nsSize prefSize = childBox->GetXULPrefSize(state);
    nsSize minSize = childBox->GetXULMinSize(state);
    nsSize maxSize =
        nsIFrame::XULBoundsCheckMinMax(minSize, childBox->GetXULMaxSize(state));
    prefSize = nsIFrame::XULBoundsCheck(minSize, prefSize, maxSize);

    nsSplitterFrame::AddXULMargin(childBox, minSize);
    nsSplitterFrame::AddXULMargin(childBox, prefSize);
    nsSplitterFrame::AddXULMargin(childBox, maxSize);

    nscoord flex = childBox->GetXULFlex();

    nsMargin margin;
    childBox->GetXULMargin(margin);
    nsRect r(childBox->GetRect());
    r.Inflate(margin);
    if (!foundOuter && (resizeBefore != Flex || flex > 0)) {
      mChildInfosBefore[mChildInfosBeforeCount].childElem = content;
      mChildInfosBefore[mChildInfosBeforeCount].min =
          isHorizontal ? minSize.width : minSize.height;
      mChildInfosBefore[mChildInfosBeforeCount].max =
          isHorizontal ? maxSize.width : maxSize.height;
      mChildInfosBefore[mChildInfosBeforeCount].current =
          isHorizontal ? r.width : r.height;
      mChildInfosBefore[mChildInfosBeforeCount].flex = flex;
      mChildInfosBefore[mChildInfosBeforeCount].changed =
          mChildInfosBefore[mChildInfosBeforeCount].current;
      mChildInfosBeforeCount++;
    } else if (foundOuter && (resizeAfter != Flex || flex > 0)) {
      mChildInfosAfter[mChildInfosAfterCount].childElem = content;
      mChildInfosAfter[mChildInfosAfterCount].min =
          isHorizontal ? minSize.width : minSize.height;
      mChildInfosAfter[mChildInfosAfterCount].max =
          isHorizontal ? maxSize.width : maxSize.height;
      mChildInfosAfter[mChildInfosAfterCount].current =
          isHorizontal ? r.width : r.height;
      mChildInfosAfter[mChildInfosAfterCount].flex = flex;
      mChildInfosAfter[mChildInfosAfterCount].changed =
          mChildInfosAfter[mChildInfosAfterCount].current;
      mChildInfosAfterCount++;
    }
  }

  if (!foundOuter) {
    return NS_OK;
  }

  mPressed = true;

  if (!mParentBox->IsXULNormalDirection()) {
    // The before array is really the after array, and the order needs to be
    // reversed. First reverse both arrays.
    Reverse(mChildInfosBefore, mChildInfosBeforeCount);
    Reverse(mChildInfosAfter, mChildInfosAfterCount);

    // Now swap the two arrays.
    std::swap(mChildInfosBeforeCount, mChildInfosAfterCount);
    std::swap(mChildInfosBefore, mChildInfosAfter);
  }

  // if resizebefore is not Farthest, reverse the list because the first child
  // in the list is the farthest, and we want the first child to be the closest.
  if (resizeBefore != Farthest)
    Reverse(mChildInfosBefore, mChildInfosBeforeCount);

  // if the resizeafter is the Farthest we must reverse the list because the
  // first child in the list is the closest we want the first child to be the
  // Farthest.
  if (resizeAfter == Farthest) Reverse(mChildInfosAfter, mChildInfosAfterCount);

  // grow only applies to the children after. If grow is set then no space
  // should be taken out of any children after us. To do this we just set the
  // size of that list to be 0.
  if (resizeAfter == Grow) mChildInfosAfterCount = 0;

  int32_t c;
  nsPoint pt =
      nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(mouseEvent, mParentBox);
  if (isHorizontal) {
    c = pt.x;
    mSplitterPos = mOuter->mRect.x;
  } else {
    c = pt.y;
    mSplitterPos = mOuter->mRect.y;
  }

  mDragStart = c;

  // printf("Pressed mDragStart=%d\n",mDragStart);

  PresShell::SetCapturingContent(mOuter->GetContent(),
                                 CaptureFlags::IgnoreAllowedState);

  return NS_OK;
}

nsresult nsSplitterFrameInner::MouseMove(Event* aMouseEvent) {
  NS_ENSURE_TRUE(mOuter, NS_OK);
  if (!mPressed) {
    return NS_OK;
  }

  if (mDragging) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventListener> kungfuDeathGrip(this);
  mOuter->mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::state,
                                         u"dragging"_ns, true);

  RemoveListener();
  mDragging = true;

  return NS_OK;
}

void nsSplitterFrameInner::Reverse(UniquePtr<nsSplitterInfo[]>& aChildInfos,
                                   int32_t aCount) {
  UniquePtr<nsSplitterInfo[]> infos(new nsSplitterInfo[aCount]);

  for (int i = 0; i < aCount; i++) infos[i] = aChildInfos[aCount - 1 - i];

  aChildInfos = std::move(infos);
}

bool nsSplitterFrameInner::SupportsCollapseDirection(
    nsSplitterFrameInner::CollapseDirection aDirection) {
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::before, nsGkAtoms::after, nsGkAtoms::both, nullptr};

  switch (SplitterElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::collapse, strings, eCaseMatters)) {
    case 0:
      return (aDirection == Before);
    case 1:
      return (aDirection == After);
    case 2:
      return true;
  }

  return false;
}

void nsSplitterFrameInner::UpdateState() {
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
    const bool prev = newState == CollapsedBefore || mState == CollapsedBefore;
    nsIFrame* splitterSibling =
        nsBoxFrame::SlowOrdinalGroupAwareSibling(mOuter, !prev);
    if (splitterSibling) {
      nsCOMPtr<nsIContent> sibling = splitterSibling->GetContent();
      if (sibling && sibling->IsElement()) {
        if (mState == CollapsedBefore || mState == CollapsedAfter) {
          // CollapsedBefore -> Open
          // CollapsedBefore -> Dragging
          // CollapsedAfter -> Open
          // CollapsedAfter -> Dragging
          nsContentUtils::AddScriptRunner(new nsUnsetAttrRunnable(
              sibling->AsElement(), nsGkAtoms::collapsed));
        } else if ((mState == Open || mState == Dragging) &&
                   (newState == CollapsedBefore ||
                    newState == CollapsedAfter)) {
          // Open -> CollapsedBefore / CollapsedAfter
          // Dragging -> CollapsedBefore / CollapsedAfter
          nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
              sibling->AsElement(), nsGkAtoms::collapsed, u"true"_ns));
        }
      }
    }
  }
  mState = newState;
}

void nsSplitterFrameInner::EnsureOrient() {
  bool isHorizontal = !mParentBox->HasAnyStateBits(NS_STATE_IS_HORIZONTAL);
  if (isHorizontal)
    mOuter->AddStateBits(NS_STATE_IS_HORIZONTAL);
  else
    mOuter->RemoveStateBits(NS_STATE_IS_HORIZONTAL);
}

void nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext) {
  EnsureOrient();
  bool isHorizontal = !mOuter->IsXULHorizontal();

  AdjustChildren(aPresContext, mChildInfosBefore.get(), mChildInfosBeforeCount,
                 isHorizontal);
  AdjustChildren(aPresContext, mChildInfosAfter.get(), mChildInfosAfterCount,
                 isHorizontal);
}

static nsIFrame* GetChildBoxForContent(nsIFrame* aParentBox,
                                       nsIContent* aContent) {
  nsIFrame* childBox = nsIFrame::GetChildXULBox(aParentBox);

  while (childBox) {
    if (childBox->GetContent() == aContent) {
      return childBox;
    }
    childBox = nsIFrame::GetNextXULBox(childBox);
  }
  return nullptr;
}

void nsSplitterFrameInner::AdjustChildren(nsPresContext* aPresContext,
                                          nsSplitterInfo* aChildInfos,
                                          int32_t aCount, bool aIsHorizontal) {
  /// printf("------- AdjustChildren------\n");

  nsBoxLayoutState state(aPresContext);

  // first set all the widths.
  nsIFrame* child = nsIFrame::GetChildXULBox(mOuter);
  while (child) {
    SetPreferredSize(state, child, aIsHorizontal, nullptr);
    child = nsIFrame::GetNextXULBox(child);
  }

  // now set our changed widths.
  for (int i = 0; i < aCount; i++) {
    nscoord pref = aChildInfos[i].changed;
    nsIFrame* childBox =
        GetChildBoxForContent(mParentBox, aChildInfos[i].childElem);

    if (childBox) {
      SetPreferredSize(state, childBox, aIsHorizontal, &pref);
    }
  }
}

void nsSplitterFrameInner::SetPreferredSize(nsBoxLayoutState& aState,
                                            nsIFrame* aChildBox,
                                            bool aIsHorizontal,
                                            nscoord* aSize) {
  nsRect rect(aChildBox->GetRect());
  nscoord pref = 0;

  if (!aSize) {
    if (aIsHorizontal)
      pref = rect.width;
    else
      pref = rect.height;
  } else {
    pref = *aSize;
  }

  nsMargin margin(0, 0, 0, 0);
  aChildBox->GetXULMargin(margin);

  if (aIsHorizontal) {
    pref -= (margin.left + margin.right);
  } else {
    pref -= (margin.top + margin.bottom);
  }

  RefPtr element = nsStyledElement::FromNode(aChildBox->GetContent());
  if (!element) {
    return;
  }

  // We set both the attribute and the CSS value, so that XUL persist="" keeps
  // working, see bug 1790712.

  int32_t pixels = pref / AppUnitsPerCSSPixel();
  nsAutoString attrValue;
  attrValue.AppendInt(pixels);
  element->SetAttr(aIsHorizontal ? nsGkAtoms::width : nsGkAtoms::height,
                   attrValue, IgnoreErrors());

  nsCOMPtr<nsICSSDeclaration> decl = element->Style();

  nsAutoCString cssValue;
  cssValue.AppendInt(pixels);
  cssValue.AppendLiteral("px");
  decl->SetProperty(aIsHorizontal ? "width"_ns : "height"_ns, cssValue, ""_ns,
                    IgnoreErrors());
}

void nsSplitterFrameInner::AddRemoveSpace(nscoord aDiff,
                                          nsSplitterInfo* aChildInfos,
                                          int32_t aCount, int32_t& aSpaceLeft) {
  aSpaceLeft = 0;

  for (int i = 0; i < aCount; i++) {
    nscoord min = aChildInfos[i].min;
    nscoord max = aChildInfos[i].max;
    nscoord& c = aChildInfos[i].changed;

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
    if (aDiff == 0) break;
  }

  aSpaceLeft = aDiff;
}

/**
 * Ok if we want to resize a child we will know the actual size in pixels we
 * want it to be. This is not the preferred size. But they only way we can
 * change a child is my manipulating its preferred size. So give the actual
 * pixel size this return method will return figure out the preferred size and
 * set it.
 */

void nsSplitterFrameInner::ResizeChildTo(nscoord& aDiff,
                                         nsSplitterInfo* aChildrenBeforeInfos,
                                         nsSplitterInfo* aChildrenAfterInfos,
                                         int32_t aChildrenBeforeCount,
                                         int32_t aChildrenAfterCount,
                                         bool aBounded) {
  nscoord spaceLeft;
  AddRemoveSpace(aDiff, aChildrenBeforeInfos, aChildrenBeforeCount, spaceLeft);

  // if there is any space left over remove it from the dif we were originally
  // given
  aDiff -= spaceLeft;
  AddRemoveSpace(-aDiff, aChildrenAfterInfos, aChildrenAfterCount, spaceLeft);

  if (spaceLeft != 0) {
    if (aBounded) {
      aDiff += spaceLeft;
      AddRemoveSpace(spaceLeft, aChildrenBeforeInfos, aChildrenBeforeCount,
                     spaceLeft);
    }
  }
}
