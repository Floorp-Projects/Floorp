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
#include "nsLayoutUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/StaticPrefs_apz.h"

using namespace mozilla;
using mozilla::dom::Element;

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
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsScrollbarFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // We want to be a reflow root since we use reflows to move the
  // slider.  Any reflow inside the scrollbar frame will be a reflow to
  // move the slider and will thus not change anything outside of the
  // scrollbar or change the size of the scrollbar frame.
  AddStateBits(NS_FRAME_REFLOW_ROOT);
}

void nsScrollbarFrame::Destroy(DestroyContext& aContext) {
  aContext.AddAnonymousContent(mUpTopButton.forget());
  aContext.AddAnonymousContent(mDownTopButton.forget());
  aContext.AddAnonymousContent(mSlider.forget());
  aContext.AddAnonymousContent(mUpBottomButton.forget());
  aContext.AddAnonymousContent(mDownBottomButton.forget());
  nsContainerFrame::Destroy(aContext);
}

void nsScrollbarFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MarkInReflow();
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // We always take all the space we're given, and our track size in the other
  // axis.
  const bool horizontal = IsHorizontal();
  const auto wm = GetWritingMode();
  const auto minSize = aReflowInput.ComputedMinSize();

  aDesiredSize.ISize(wm) = aReflowInput.ComputedISize();
  aDesiredSize.BSize(wm) = [&] {
    if (aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      return aReflowInput.ComputedBSize();
    }
    // We don't want to change our size during incremental reflow, see the
    // reflow root comment in init.
    if (!aReflowInput.mParentReflowInput) {
      return GetLogicalSize(wm).BSize(wm);
    }
    return minSize.BSize(wm);
  }();

  const nsSize containerSize = aDesiredSize.PhysicalSize();
  const LogicalSize totalAvailSize = aDesiredSize.Size(wm);
  LogicalPoint nextKidPos(wm);

  MOZ_ASSERT(!wm.IsVertical());
  const bool movesInInlineDirection = horizontal;

  // Layout our kids left to right / top to bottom.
  for (nsIFrame* kid : mFrames) {
    MOZ_ASSERT(!kid->GetWritingMode().IsOrthogonalTo(wm),
               "We don't expect orthogonal scrollbar parts");
    const bool isSlider = kid->GetContent() == mSlider;
    LogicalSize availSize = totalAvailSize;
    {
      // Assume we'll consume the same size before and after the slider. This is
      // not a technically correct assumption if we have weird scrollbar button
      // setups, but those will be going away, see bug 1824254.
      const int32_t factor = isSlider ? 2 : 1;
      if (movesInInlineDirection) {
        availSize.ISize(wm) =
            std::max(0, totalAvailSize.ISize(wm) - nextKidPos.I(wm) * factor);
      } else {
        availSize.BSize(wm) =
            std::max(0, totalAvailSize.BSize(wm) - nextKidPos.B(wm) * factor);
      }
    }

    ReflowInput kidRI(aPresContext, aReflowInput, kid, availSize);
    if (isSlider) {
      // We want for the slider to take all the remaining available space.
      kidRI.SetComputedISize(availSize.ISize(wm));
      kidRI.SetComputedBSize(availSize.BSize(wm));
    } else if (movesInInlineDirection) {
      // Otherwise we want all the space in the axis we're not advancing in, and
      // the default / minimum size on the other axis.
      kidRI.SetComputedBSize(availSize.BSize(wm));
    } else {
      kidRI.SetComputedISize(availSize.ISize(wm));
    }

    ReflowOutput kidDesiredSize(wm);
    nsReflowStatus status;
    const auto flags = ReflowChildFlags::Default;
    ReflowChild(kid, aPresContext, kidDesiredSize, kidRI, wm, nextKidPos,
                containerSize, flags, status);
    // We haven't seen the slider yet, we can advance
    FinishReflowChild(kid, aPresContext, kidDesiredSize, &kidRI, wm, nextKidPos,
                      containerSize, flags);
    if (movesInInlineDirection) {
      nextKidPos.I(wm) += kidDesiredSize.ISize(wm);
    } else {
      nextKidPos.B(wm) += kidDesiredSize.BSize(wm);
    }
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();
}

nsresult nsScrollbarFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  nsresult rv =
      nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  // Update value in our children
  UpdateChildrenAttributeValue(aAttribute, true);

  // if the current position changes, notify any nsGfxScrollFrame
  // parent we may have
  if (aAttribute != nsGkAtoms::curpos) {
    return rv;
  }

  nsIScrollableFrame* scrollable = do_QueryFrame(GetParent());
  if (!scrollable) {
    return rv;
  }

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

bool nsScrollbarFrame::IsHorizontal() const {
  auto appearance = StyleDisplay()->EffectiveAppearance();
  MOZ_ASSERT(appearance == StyleAppearance::ScrollbarHorizontal ||
             appearance == StyleAppearance::ScrollbarVertical);
  return appearance == StyleAppearance::ScrollbarHorizontal;
}

nsSize nsScrollbarFrame::ScrollbarMinSize() const {
  nsPresContext* pc = PresContext();
  const LayoutDeviceIntSize widget =
      pc->Theme()->GetMinimumWidgetSize(pc, const_cast<nsScrollbarFrame*>(this),
                                        StyleDisplay()->EffectiveAppearance());
  return LayoutDeviceIntSize::ToAppUnits(widget, pc->AppUnitsPerDevPixel());
}

StyleScrollbarWidth nsScrollbarFrame::ScrollbarWidth() const {
  return nsLayoutUtils::StyleForScrollbar(this)
      ->StyleUIReset()
      ->ScrollbarWidth();
}

nscoord nsScrollbarFrame::ScrollbarTrackSize() const {
  nsPresContext* pc = PresContext();
  auto overlay = pc->UseOverlayScrollbars() ? nsITheme::Overlay::Yes
                                            : nsITheme::Overlay::No;
  return LayoutDevicePixel::ToAppUnits(
      pc->Theme()->GetScrollbarSize(pc, ScrollbarWidth(), overlay),
      pc->AppUnitsPerDevPixel());
}

void nsScrollbarFrame::SetIncrementToLine(int32_t aDirection) {
  mSmoothScroll = true;
  mDirection = aDirection;
  mScrollUnit = ScrollUnit::LINES;

  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mIncrement = aDirection * nsSliderFrame::GetIncrement(content);
}

void nsScrollbarFrame::SetIncrementToPage(int32_t aDirection) {
  mSmoothScroll = true;
  mDirection = aDirection;
  mScrollUnit = ScrollUnit::PAGES;

  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mIncrement = aDirection * nsSliderFrame::GetPageIncrement(content);
}

void nsScrollbarFrame::SetIncrementToWhole(int32_t aDirection) {
  // Don't repeat or use smooth scrolling if scrolling to beginning or end
  // of a page.
  mSmoothScroll = false;
  mDirection = aDirection;
  mScrollUnit = ScrollUnit::WHOLE;

  // get the scrollbar's content node
  nsIContent* content = GetContent();
  if (aDirection == -1)
    mIncrement = -nsSliderFrame::GetCurrentPosition(content);
  else
    mIncrement = nsSliderFrame::GetMaxPosition(content) -
                 nsSliderFrame::GetCurrentPosition(content);
}

int32_t nsScrollbarFrame::MoveToNewPosition(
    ImplementsScrollByUnit aImplementsScrollByUnit) {
  if (aImplementsScrollByUnit == ImplementsScrollByUnit::Yes &&
      StaticPrefs::apz_scrollbarbuttonrepeat_enabled()) {
    nsIScrollbarMediator* m = GetScrollbarMediator();
    MOZ_ASSERT(m);
    // aImplementsScrollByUnit being Yes indicates the caller doesn't care
    // about the return value.
    // Note that this `MoveToNewPosition` is used for scrolling triggered by
    // repeating scrollbar button press, so we'd use an intended-direction
    // scroll snap flag.
    m->ScrollByUnit(
        this, mSmoothScroll ? ScrollMode::Smooth : ScrollMode::Instant,
        mDirection, mScrollUnit, ScrollSnapFlags::IntendedDirection);
    return 0;
  }

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
    content->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth, u"true"_ns, false);
  }
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curposStr, false);
  // notify the nsScrollbarFrame of the change
  AttributeChanged(kNameSpaceID_None, nsGkAtoms::curpos,
                   dom::MutationEvent_Binding::MODIFICATION);
  if (!weakFrame.IsAlive()) {
    return curpos;
  }
  // notify all nsSliderFrames of the change
  for (const auto& childList : ChildLists()) {
    for (nsIFrame* f : childList.mList) {
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

static already_AddRefed<Element> MakeScrollbarButton(
    dom::NodeInfo* aNodeInfo, bool aVertical, bool aBottom, bool aDown,
    AnonymousContentKey& aKey) {
  MOZ_ASSERT(aNodeInfo);
  MOZ_ASSERT(
      aNodeInfo->Equals(nsGkAtoms::scrollbarbutton, nullptr, kNameSpaceID_XUL));

  static constexpr nsLiteralString kSbattrValues[2][2] = {
      {
          u"scrollbar-up-top"_ns,
          u"scrollbar-up-bottom"_ns,
      },
      {
          u"scrollbar-down-top"_ns,
          u"scrollbar-down-bottom"_ns,
      },
  };

  static constexpr nsLiteralString kTypeValues[2] = {
      u"decrement"_ns,
      u"increment"_ns,
  };

  aKey = AnonymousContentKey::Type_ScrollbarButton;
  if (aVertical) {
    aKey |= AnonymousContentKey::Flag_Vertical;
  }
  if (aBottom) {
    aKey |= AnonymousContentKey::Flag_ScrollbarButton_Bottom;
  }
  if (aDown) {
    aKey |= AnonymousContentKey::Flag_ScrollbarButton_Down;
  }

  RefPtr<Element> e;
  NS_TrustedNewXULElement(getter_AddRefs(e), do_AddRef(aNodeInfo));
  e->SetAttr(kNameSpaceID_None, nsGkAtoms::sbattr,
             kSbattrValues[aDown][aBottom], false);
  e->SetAttr(kNameSpaceID_None, nsGkAtoms::type, kTypeValues[aDown], false);
  return e.forget();
}

nsresult nsScrollbarFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  nsNodeInfoManager* nodeInfoManager = mContent->NodeInfo()->NodeInfoManager();
  Element* el = GetContent()->AsElement();

  // If there are children already in the node, don't create any anonymous
  // content (this only apply to crashtests/369038-1.xhtml)
  if (el->HasChildren()) {
    return NS_OK;
  }

  nsAutoString orient;
  el->GetAttr(nsGkAtoms::orient, orient);
  bool vertical = orient.EqualsLiteral("vertical");

  RefPtr<dom::NodeInfo> sbbNodeInfo =
      nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbarbutton, nullptr,
                                   kNameSpaceID_XUL, nsINode::ELEMENT_NODE);

  bool createButtons = PresContext()->Theme()->ThemeSupportsScrollbarButtons();

  if (createButtons) {
    AnonymousContentKey key;
    mUpTopButton =
        MakeScrollbarButton(sbbNodeInfo, vertical, /* aBottom */ false,
                            /* aDown */ false, key);
    aElements.AppendElement(ContentInfo(mUpTopButton, key));
  }

  if (createButtons) {
    AnonymousContentKey key;
    mDownTopButton =
        MakeScrollbarButton(sbbNodeInfo, vertical, /* aBottom */ false,
                            /* aDown */ true, key);
    aElements.AppendElement(ContentInfo(mDownTopButton, key));
  }

  {
    AnonymousContentKey key = AnonymousContentKey::Type_Slider;
    if (vertical) {
      key |= AnonymousContentKey::Flag_Vertical;
    }

    NS_TrustedNewXULElement(
        getter_AddRefs(mSlider),
        nodeInfoManager->GetNodeInfo(nsGkAtoms::slider, nullptr,
                                     kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
    mSlider->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, orient, false);

    aElements.AppendElement(ContentInfo(mSlider, key));

    NS_TrustedNewXULElement(
        getter_AddRefs(mThumb),
        nodeInfoManager->GetNodeInfo(nsGkAtoms::thumb, nullptr,
                                     kNameSpaceID_XUL, nsINode::ELEMENT_NODE));
    mThumb->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, orient, false);
    mSlider->AppendChildTo(mThumb, false, IgnoreErrors());
  }

  if (createButtons) {
    AnonymousContentKey key;
    mUpBottomButton =
        MakeScrollbarButton(sbbNodeInfo, vertical, /* aBottom */ true,
                            /* aDown */ false, key);
    aElements.AppendElement(ContentInfo(mUpBottomButton, key));
  }

  if (createButtons) {
    AnonymousContentKey key;
    mDownBottomButton =
        MakeScrollbarButton(sbbNodeInfo, vertical, /* aBottom */ true,
                            /* aDown */ true, key);
    aElements.AppendElement(ContentInfo(mDownBottomButton, key));
  }

  // Don't cache styles if we are inside a <select> element, since we have
  // some UA style sheet rules that depend on the <select>'s attributes.
  if (GetContent()->GetParent() &&
      GetContent()->GetParent()->IsHTMLElement(nsGkAtoms::select)) {
    for (auto& info : aElements) {
      info.mKey = AnonymousContentKey::None;
    }
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
  Element* el = GetContent()->AsElement();

  nsAutoString value;
  el->GetAttr(aAttribute, value);

  if (!el->HasAttr(aAttribute)) {
    if (mUpTopButton) {
      mUpTopButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mDownTopButton) {
      mDownTopButton->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
    }
    if (mSlider) {
      mSlider->UnsetAttr(kNameSpaceID_None, aAttribute, aNotify);
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
