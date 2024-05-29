/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/ElementAnimationData.h"
#include "mozilla/PresShell.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"

namespace mozilla::dom {

// ---------------------------------
// Methods of ScrollTimeline
// ---------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(ScrollTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScrollTimeline,
                                                AnimationTimeline)
  tmp->Teardown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScrollTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ScrollTimeline,
                                               AnimationTimeline)

ScrollTimeline::ScrollTimeline(Document* aDocument, const Scroller& aScroller,
                               StyleScrollAxis aAxis)
    : AnimationTimeline(aDocument->GetParentObject(),
                        aDocument->GetScopeObject()->GetRTPCallerType()),
      mDocument(aDocument),
      mSource(aScroller),
      mAxis(aAxis) {
  MOZ_ASSERT(aDocument);
  RegisterWithScrollSource();
}

/* static */ std::pair<const Element*, PseudoStyleType>
ScrollTimeline::FindNearestScroller(Element* aSubject,
                                    PseudoStyleType aPseudoType) {
  MOZ_ASSERT(aSubject);
  Element* subject =
      AnimationUtils::GetElementForRestyle(aSubject, aPseudoType);

  Element* curr = subject->GetFlattenedTreeParentElement();
  Element* root = subject->OwnerDoc()->GetDocumentElement();
  while (curr && curr != root) {
    const ComputedStyle* style = Servo_Element_GetMaybeOutOfDateStyle(curr);
    MOZ_ASSERT(style, "The ancestor should be styled.");
    if (style->StyleDisplay()->IsScrollableOverflow()) {
      break;
    }
    curr = curr->GetFlattenedTreeParentElement();
  }
  // If there is no scroll container, we use root.
  if (!curr) {
    return {root, PseudoStyleType::NotPseudo};
  }
  return AnimationUtils::GetElementPseudoPair(curr);
}

/* static */
already_AddRefed<ScrollTimeline> ScrollTimeline::MakeAnonymous(
    Document* aDocument, const NonOwningAnimationTarget& aTarget,
    StyleScrollAxis aAxis, StyleScroller aScroller) {
  MOZ_ASSERT(aTarget);
  Scroller scroller;
  switch (aScroller) {
    case StyleScroller::Root:
      scroller = Scroller::Root(aTarget.mElement->OwnerDoc());
      break;

    case StyleScroller::Nearest: {
      auto [element, pseudo] =
          FindNearestScroller(aTarget.mElement, aTarget.mPseudoType);
      scroller = Scroller::Nearest(const_cast<Element*>(element), pseudo);
      break;
    }
    case StyleScroller::SelfElement:
      scroller = Scroller::Self(aTarget.mElement, aTarget.mPseudoType);
      break;
  }

  // Each use of scroll() corresponds to its own instance of ScrollTimeline in
  // the Web Animations API, even if multiple elements use scroll() to refer to
  // the same scroll container with the same arguments.
  // https://drafts.csswg.org/scroll-animations-1/#scroll-notation
  return MakeAndAddRef<ScrollTimeline>(aDocument, scroller, aAxis);
}

/* static*/ already_AddRefed<ScrollTimeline> ScrollTimeline::MakeNamed(
    Document* aDocument, Element* aReferenceElement,
    PseudoStyleType aPseudoType, const StyleScrollTimeline& aStyleTimeline) {
  MOZ_ASSERT(NS_IsMainThread());

  Scroller scroller = Scroller::Named(aReferenceElement, aPseudoType);
  return MakeAndAddRef<ScrollTimeline>(aDocument, std::move(scroller),
                                       aStyleTimeline.GetAxis());
}

Nullable<TimeDuration> ScrollTimeline::GetCurrentTimeAsDuration() const {
  // If no layout box, this timeline is inactive.
  if (!mSource || !mSource.mElement->GetPrimaryFrame()) {
    return nullptr;
  }

  // if this is not a scroller container, this timeline is inactive.
  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  if (!scrollFrame) {
    return nullptr;
  }

  const auto orientation = Axis();

  // If there is no scrollable overflow, then the ScrollTimeline is inactive.
  // https://drafts.csswg.org/scroll-animations-1/#scrolltimeline-interface
  if (!scrollFrame->GetAvailableScrollingDirections().contains(orientation)) {
    return nullptr;
  }

  const bool isHorizontal = orientation == layers::ScrollDirection::eHorizontal;
  const nsPoint& scrollPosition = scrollFrame->GetScrollPosition();
  const Maybe<ScrollOffsets>& offsets =
      ComputeOffsets(scrollFrame, orientation);
  if (!offsets) {
    return nullptr;
  }

  // Note: For RTL, scrollPosition.x or scrollPosition.y may be negative,
  // e.g. the range of its value is [0, -range], so we have to use the
  // absolute value.
  nscoord position =
      std::abs(isHorizontal ? scrollPosition.x : scrollPosition.y);
  double progress = static_cast<double>(position - offsets->mStart) /
                    static_cast<double>(offsets->mEnd - offsets->mStart);
  return TimeDuration::FromMilliseconds(progress *
                                        PROGRESS_TIMELINE_DURATION_MILLISEC);
}

layers::ScrollDirection ScrollTimeline::Axis() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const WritingMode wm = mSource.mElement->GetPrimaryFrame()->GetWritingMode();
  return mAxis == StyleScrollAxis::Horizontal ||
                 (!wm.IsVertical() && mAxis == StyleScrollAxis::Inline) ||
                 (wm.IsVertical() && mAxis == StyleScrollAxis::Block)
             ? layers::ScrollDirection::eHorizontal
             : layers::ScrollDirection::eVertical;
}

StyleOverflow ScrollTimeline::SourceScrollStyle() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  MOZ_ASSERT(scrollFrame);

  const ScrollStyles scrollStyles = scrollFrame->GetScrollStyles();

  return Axis() == layers::ScrollDirection::eHorizontal
             ? scrollStyles.mHorizontal
             : scrollStyles.mVertical;
}

bool ScrollTimeline::APZIsActiveForSource() const {
  MOZ_ASSERT(mSource);
  return gfxPlatform::AsyncPanZoomEnabled() &&
         !nsLayoutUtils::ShouldDisableApzForElement(mSource.mElement) &&
         DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(mSource.mElement);
}

bool ScrollTimeline::ScrollingDirectionIsAvailable() const {
  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  MOZ_ASSERT(scrollFrame);
  return scrollFrame->GetAvailableScrollingDirections().contains(Axis());
}

void ScrollTimeline::ReplacePropertiesWith(const Element* aReferenceElement,
                                           PseudoStyleType aPseudoType,
                                           const StyleScrollTimeline& aNew) {
  MOZ_ASSERT(aReferenceElement == mSource.mElement &&
             aPseudoType == mSource.mPseudoType);
  mAxis = aNew.GetAxis();

  for (auto* anim = mAnimationOrder.getFirst(); anim;
       anim = static_cast<LinkedListElement<Animation>*>(anim)->getNext()) {
    MOZ_ASSERT(anim->GetTimeline() == this);
    // Set this so we just PostUpdate() for this animation.
    anim->SetTimeline(this);
  }
}

Maybe<ScrollTimeline::ScrollOffsets> ScrollTimeline::ComputeOffsets(
    const nsIScrollableFrame* aScrollFrame,
    layers::ScrollDirection aOrientation) const {
  const nsRect& scrollRange = aScrollFrame->GetScrollRange();
  nscoord range = aOrientation == layers::ScrollDirection::eHorizontal
                      ? scrollRange.width
                      : scrollRange.height;
  MOZ_ASSERT(range > 0);
  return Some(ScrollOffsets{0, range});
}

void ScrollTimeline::RegisterWithScrollSource() {
  if (!mSource) {
    return;
  }

  auto& scheduler =
      ProgressTimelineScheduler::Ensure(mSource.mElement, mSource.mPseudoType);
  scheduler.AddTimeline(this);
}

void ScrollTimeline::UnregisterFromScrollSource() {
  if (!mSource) {
    return;
  }

  auto* scheduler =
      ProgressTimelineScheduler::Get(mSource.mElement, mSource.mPseudoType);
  if (!scheduler) {
    return;
  }

  scheduler->RemoveTimeline(this);
  if (scheduler->IsEmpty()) {
    ProgressTimelineScheduler::Destroy(mSource.mElement, mSource.mPseudoType);
  }
}

const nsIScrollableFrame* ScrollTimeline::GetScrollFrame() const {
  if (!mSource) {
    return nullptr;
  }

  switch (mSource.mType) {
    case Scroller::Type::Root:
      if (const PresShell* presShell =
              mSource.mElement->OwnerDoc()->GetPresShell()) {
        return presShell->GetRootScrollFrameAsScrollable();
      }
      return nullptr;
    case Scroller::Type::Nearest:
    case Scroller::Type::Name:
    case Scroller::Type::Self:
      return nsLayoutUtils::FindScrollableFrameFor(mSource.mElement);
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported scroller type");
  return nullptr;
}

void ScrollTimeline::NotifyAnimationContentVisibilityChanged(
    Animation* aAnimation, bool aIsVisible) {
  AnimationTimeline::NotifyAnimationContentVisibilityChanged(aAnimation,
                                                             aIsVisible);
  if (mAnimationOrder.isEmpty()) {
    UnregisterFromScrollSource();
  } else {
    RegisterWithScrollSource();
  }
}

// ------------------------------------
// Methods of ProgressTimelineScheduler
// ------------------------------------
/* static */ ProgressTimelineScheduler* ProgressTimelineScheduler::Get(
    const Element* aElement, PseudoStyleType aPseudoType) {
  MOZ_ASSERT(aElement);
  auto* data = aElement->GetAnimationData();
  if (!data) {
    return nullptr;
  }

  return data->GetProgressTimelineScheduler(aPseudoType);
}

/* static */ ProgressTimelineScheduler& ProgressTimelineScheduler::Ensure(
    Element* aElement, PseudoStyleType aPseudoType) {
  MOZ_ASSERT(aElement);
  return aElement->EnsureAnimationData().EnsureProgressTimelineScheduler(
      *aElement, aPseudoType);
}

/* static */
void ProgressTimelineScheduler::Destroy(const Element* aElement,
                                        PseudoStyleType aPseudoType) {
  auto* data = aElement->GetAnimationData();
  MOZ_ASSERT(data);
  data->ClearProgressTimelineScheduler(aPseudoType);
}

}  // namespace mozilla::dom
