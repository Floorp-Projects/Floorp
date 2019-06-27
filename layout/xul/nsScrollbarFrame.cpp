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

#include "nsScrollbarFrame.h"
#include "nsSliderFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/MutationEventBinding.h"

using namespace mozilla;

//
// NS_NewScrollbarFrame
//
// Creates a new scrollbar frame and returns it
//
nsIFrame* NS_NewScrollbarFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsScrollbarFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsScrollbarFrame)

NS_QUERYFRAME_HEAD(nsScrollbarFrame)
  NS_QUERYFRAME_ENTRY(nsScrollbarFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

void nsScrollbarFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // We want to be a reflow root since we use reflows to move the
  // slider.  Any reflow inside the scrollbar frame will be a reflow to
  // move the slider and will thus not change anything outside of the
  // scrollbar or change the size of the scrollbar frame.
  AddStateBits(NS_FRAME_REFLOW_ROOT);
}

void nsScrollbarFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                   PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mUpTopButton.forget());
  aPostDestroyData.AddAnonymousContent(mDownTopButton.forget());
  aPostDestroyData.AddAnonymousContent(mSlider.forget());
  aPostDestroyData.AddAnonymousContent(mUpBottomButton.forget());
  aPostDestroyData.AddAnonymousContent(mDownBottomButton.forget());
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsScrollbarFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);

  // nsGfxScrollFrame may have told us to shrink to nothing. If so, make sure
  // our desired size agrees.
  if (aReflowInput.AvailableWidth() == 0) {
    aDesiredSize.Width() = 0;
  }
  if (aReflowInput.AvailableHeight() == 0) {
    aDesiredSize.Height() = 0;
  }
}

nsresult nsScrollbarFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  nsresult rv =
      nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  // Update value in our children
  UpdateChildrenAttributeValue(aAttribute, true);

  // if the current position changes, notify any nsGfxScrollFrame
  // parent we may have
  if (aAttribute != nsGkAtoms::curpos) return rv;

  nsIScrollableFrame* scrollable = do_QueryFrame(GetParent());
  if (!scrollable) return rv;

  nsCOMPtr<nsIContent> content(mContent);
  scrollable->CurPosAttributeChanged(content);
  return rv;
}

NS_IMETHODIMP
nsScrollbarFrame::HandlePress(nsPresContext* aPresContext,
                              WidgetGUIEvent* aEvent,
                              nsEventStatus* aEventStatus) {
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleMultiplePress(nsPresContext* aPresContext,
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus,
                                      bool aControlHeld) {
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleDrag(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus) {
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleRelease(nsPresContext* aPresContext,
                                WidgetGUIEvent* aEvent,
                                nsEventStatus* aEventStatus) {
  return NS_OK;
}

void nsScrollbarFrame::SetScrollbarMediatorContent(nsIContent* aMediator) {
  mScrollbarMediator = aMediator;
}

nsIScrollbarMediator* nsScrollbarFrame::GetScrollbarMediator() {
  if (!mScrollbarMediator) {
    return nullptr;
  }
  nsIFrame* f = mScrollbarMediator->GetPrimaryFrame();
  nsIScrollableFrame* scrollFrame = do_QueryFrame(f);
  nsIScrollbarMediator* sbm;

  if (scrollFrame) {
    nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
    sbm = do_QueryFrame(scrolledFrame);
    if (sbm) {
      return sbm;
    }
  }
  sbm = do_QueryFrame(f);
  if (f && !sbm) {
    f = f->PresShell()->GetRootScrollFrame();
    if (f && f->GetContent() == mScrollbarMediator) {
      return do_QueryFrame(f);
    }
  }
  return sbm;
}

nsresult nsScrollbarFrame::GetXULMargin(nsMargin& aMargin) {
  nsresult rv = NS_ERROR_FAILURE;
  aMargin.SizeTo(0, 0, 0, 0);

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    nsPresContext* presContext = PresContext();
    nsITheme* theme = presContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(presContext, this,
                                            StyleAppearance::Scrollbar)) {
      LayoutDeviceIntSize size;
      bool isOverridable;
      theme->GetMinimumWidgetSize(presContext, this, StyleAppearance::Scrollbar,
                                  &size, &isOverridable);
      if (IsXULHorizontal()) {
        aMargin.top = -presContext->DevPixelsToAppUnits(size.height);
      } else {
        aMargin.left = -presContext->DevPixelsToAppUnits(size.width);
      }
      rv = NS_OK;
    }
  }

  if (NS_FAILED(rv)) {
    rv = nsBox::GetXULMargin(aMargin);
  }

  if (NS_SUCCEEDED(rv) && !IsXULHorizontal()) {
    nsIScrollbarMediator* scrollFrame = GetScrollbarMediator();
    if (scrollFrame && !scrollFrame->IsScrollbarOnRight()) {
      Swap(aMargin.left, aMargin.right);
    }
  }

  return rv;
}

void nsScrollbarFrame::SetIncrementToLine(int32_t aDirection) {
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mSmoothScroll = true;
  mIncrement = aDirection * nsSliderFrame::GetIncrement(content);
}

void nsScrollbarFrame::SetIncrementToPage(int32_t aDirection) {
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mSmoothScroll = true;
  mIncrement = aDirection * nsSliderFrame::GetPageIncrement(content);
}

void nsScrollbarFrame::SetIncrementToWhole(int32_t aDirection) {
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  if (aDirection == -1)
    mIncrement = -nsSliderFrame::GetCurrentPosition(content);
  else
    mIncrement = nsSliderFrame::GetMaxPosition(content) -
                 nsSliderFrame::GetCurrentPosition(content);
  // Don't repeat or use smooth scrolling if scrolling to beginning or end
  // of a page.
  mSmoothScroll = false;
}

int32_t nsScrollbarFrame::MoveToNewPosition() {
  // get the scrollbar's content node
  RefPtr<Element> content = GetContent()->AsElement();

  // get the current pos
  int32_t curpos = nsSliderFrame::GetCurrentPosition(content);

  // get the max pos
  int32_t maxpos = nsSliderFrame::GetMaxPosition(content);

  // increment the given amount
  if (mIncrement) {
    curpos += mIncrement;
  }

  // make sure the current position is between the current and max positions
  if (curpos < 0) {
    curpos = 0;
  } else if (curpos > maxpos) {
    curpos = maxpos;
  }

  // set the current position of the slider.
  nsAutoString curposStr;
  curposStr.AppendInt(curpos);

  AutoWeakFrame weakFrame(this);
  if (mSmoothScroll) {
    content->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth,
                     NS_LITERAL_STRING("true"), false);
  }
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curposStr, false);
  // notify the nsScrollbarFrame of the change
  AttributeChanged(kNameSpaceID_None, nsGkAtoms::curpos,
                   dom::MutationEvent_Binding::MODIFICATION);
  if (!weakFrame.IsAlive()) {
    return curpos;
  }
  // notify all nsSliderFrames of the change
  nsIFrame::ChildListIterator childLists(this);
  for (; !childLists.IsDone(); childLists.Next()) {
    nsFrameList::Enumerator childFrames(childLists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* f = childFrames.get();
      nsSliderFrame* sliderFrame = do_QueryFrame(f);
      if (sliderFrame) {
        sliderFrame->AttributeChanged(kNameSpaceID_None, nsGkAtoms::curpos,
                                      dom::MutationEvent_Binding::MODIFICATION);
        if (!weakFrame.IsAlive()) {
          return curpos;
        }
      }
    }
  }
  content->UnsetAttr(kNameSpaceID_None, nsGkAtoms::smooth, false);
  return curpos;
}

nsresult nsScrollbarFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // clang-format off

  // <xul:scrollbarbutton sbattr="scrollbar-up-top" type="decrement" xbl:inherits="curpos,maxpos,disabled"/>
  // <xul:scrollbarbutton sbattr="scrollbar-down-top" type="increment" xbl:inherits="curpos,maxpos,disabled"/>
  // <xul:slider flex="1" xbl:inherits="disabled,curpos,maxpos,pageincrement,increment,orient">
  //   <xul:thumb sbattr="scrollbar-thumb" xbl:inherits="orient,collapsed=disabled"
  //              align="center" pack="center"/>
  // </xul:slider>
  // <xul:scrollbarbutton sbattr="scrollbar-up-bottom" type="decrement" xbl:inherits="curpos,maxpos,disabled"/>
  // <xul:scrollbarbutton sbattr="scrollbar-down-bottom" type="increment" xbl:inherits="curpos,maxpos,disabled"/>

  // clang-format on

  nsNodeInfoManager* nodeInfoManager = mContent->NodeInfo()->NodeInfoManager();

  Element* el(GetContent()->AsElement());

  // If there are children already in the node, don't create any anonymous
  // content (this only apply to crashtests/369038-1.xhtml)
  if (el->HasChildren()) {
    return NS_OK;
  }

  nsAutoString orient;
  el->GetAttr(kNameSpaceID_None, nsGkAtoms::orient, orient);

  NS_TrustedNewXULElement(
      getter_AddRefs(mUpTopButton),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbarbutton, nullptr,
                                   kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
  mUpTopButton->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
                        NS_LITERAL_STRING("scrollbar-up-top"), false);
  mUpTopButton->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                        NS_LITERAL_STRING("decrement"), false);

  if (!aElements.AppendElement(mUpTopButton)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_TrustedNewXULElement(
      getter_AddRefs(mDownTopButton),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbarbutton, nullptr,
                                   kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
  mDownTopButton->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
                          NS_LITERAL_STRING("scrollbar-down-top"), false);
  mDownTopButton->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                          NS_LITERAL_STRING("increment"), false);

  if (!aElements.AppendElement(mDownTopButton)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_TrustedNewXULElement(
      getter_AddRefs(mSlider),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::slider, nullptr, kNameSpaceID_XUL,
                                   nsINode::ELEMENT_NODE));
  mSlider->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, orient, false);
  mSlider->SetAttr(kNameSpaceID_None, nsGkAtoms::flex, NS_LITERAL_STRING("1"),
                   false);

  if (!aElements.AppendElement(mSlider)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_TrustedNewXULElement(
      getter_AddRefs(mThumb),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::thumb, nullptr, kNameSpaceID_XUL,
                                   nsINode::ELEMENT_NODE));
  mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
                  NS_LITERAL_STRING("scrollbar-thumb"), false);
  mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, orient, false);
  mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::align,
                  NS_LITERAL_STRING("center"), false);
  mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::pack,
                  NS_LITERAL_STRING("center"), false);
  mSlider->AppendChildTo(mThumb, false);

  NS_TrustedNewXULElement(
      getter_AddRefs(mUpBottomButton),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbarbutton, nullptr,
                                   kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
  mUpBottomButton->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                           NS_LITERAL_STRING("decrement"), false);
  mUpBottomButton->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
                           NS_LITERAL_STRING("scrollbar-up-bottom"), false);

  if (!aElements.AppendElement(mUpBottomButton)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_TrustedNewXULElement(
      getter_AddRefs(mDownBottomButton),
      nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbarbutton, nullptr,
                                   kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
  mDownBottomButton->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
                             NS_LITERAL_STRING("scrollbar-down-bottom"), false);
  mDownBottomButton->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                             NS_LITERAL_STRING("increment"), false);

  if (!aElements.AppendElement(mDownBottomButton)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  UpdateChildrenAttributeValue(nsGkAtoms::curpos, false);
  UpdateChildrenAttributeValue(nsGkAtoms::maxpos, false);
  UpdateChildrenAttributeValue(nsGkAtoms::disabled, false);
  UpdateChildrenAttributeValue(nsGkAtoms::pageincrement, false);
  UpdateChildrenAttributeValue(nsGkAtoms::increment, false);

  return NS_OK;
}

void nsScrollbarFrame::UpdateChildrenAttributeValue(nsAtom* aAttribute,
                                                    bool aNotify) {
  Element* el(GetContent()->AsElement());

  nsAutoString value;
  el->GetAttr(kNameSpaceID_None, aAttribute, value);

  if (!el->HasAttr(kNameSpaceID_None, aAttribute)) {
    if (mUpTopButton) {
      mUpTopButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mDownTopButton) {
      mDownTopButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mSlider) {
      mSlider->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mThumb && aAttribute == nsGkAtoms::disabled) {
      mThumb->UnsetAttr(kNameSpaceID_None, nsGkAtoms::collapsed, aNotify);
    }
    if (mUpBottomButton) {
      mUpBottomButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mDownBottomButton) {
      mDownBottomButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    return;
  }

  if (aAttribute == nsGkAtoms::curpos || aAttribute == nsGkAtoms::maxpos) {
    if (mUpTopButton) {
      mUpTopButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mDownTopButton) {
      mDownTopButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mSlider) {
      mSlider->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mUpBottomButton) {
      mUpBottomButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mDownBottomButton) {
      mDownBottomButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
  } else if (aAttribute == nsGkAtoms::disabled) {
    if (mUpTopButton) {
      mUpTopButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mDownTopButton) {
      mDownTopButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mSlider) {
      mSlider->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    // Set the value on "collapsed" attribute.
    if (mThumb) {
      mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::collapsed, value, aNotify);
    }
    if (mUpBottomButton) {
      mUpBottomButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
    if (mDownBottomButton) {
      mDownBottomButton->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
  } else if (aAttribute == nsGkAtoms::pageincrement ||
             aAttribute == nsGkAtoms::increment) {
    if (mSlider) {
      mSlider->SetAttr(kNameSpaceID_None, aAttribute, value, aNotify);
    }
  }
}

void nsScrollbarFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mUpTopButton) {
    aElements.AppendElement(mUpTopButton);
  }

  if (mDownTopButton) {
    aElements.AppendElement(mDownTopButton);
  }

  if (mSlider) {
    aElements.AppendElement(mSlider);
  }

  if (mUpBottomButton) {
    aElements.AppendElement(mUpBottomButton);
  }

  if (mDownBottomButton) {
    aElements.AppendElement(mDownBottomButton);
  }
}
