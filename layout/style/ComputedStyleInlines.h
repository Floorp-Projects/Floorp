/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inlined methods for ComputedStyle. Will just redirect to
 * GeckoComputedStyle methods when compiled without stylo, but will do
 * virtual dispatch (by checking which kind of container it is)
 * in stylo mode.
 */

#ifndef ComputedStyleInlines_h
#define ComputedStyleInlines_h

#include "mozilla/ComputedStyle.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "nsStyleStructInlines.h"

namespace mozilla {

namespace detail {

template <typename T, typename Enable = void>
struct HasTriggerImageLoads : public std::false_type {};

template <typename T>
struct HasTriggerImageLoads<T, decltype(std::declval<T&>().TriggerImageLoads(
                                   std::declval<dom::Document&>(), nullptr))>
    : public std::true_type {};

template <typename T, const T* (ComputedStyle::*Method)() const>
void TriggerImageLoads(dom::Document& aDocument, const ComputedStyle* aOldStyle,
                       ComputedStyle* aStyle) {
  if constexpr (HasTriggerImageLoads<T>::value) {
    auto* old = aOldStyle ? (aOldStyle->*Method)() : nullptr;
    auto* current = const_cast<T*>((aStyle->*Method)());
    current->TriggerImageLoads(aDocument, old);
  } else {
    Unused << aOldStyle;
    Unused << aStyle;
  }
}

}  // namespace detail

void ComputedStyle::StartImageLoads(dom::Document& aDocument,
                                    const ComputedStyle* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());

#define STYLE_STRUCT(name_)                                                \
  detail::TriggerImageLoads<nsStyle##name_, &ComputedStyle::Style##name_>( \
      aDocument, aOldStyle, this);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
}

StylePointerEvents ComputedStyle::PointerEvents() const {
  if (IsRootElementStyle()) {
    // The root frame is not allowed to have pointer-events: none, or else no
    // frames could be hit test against and scrolling the viewport would not
    // work.
    return StylePointerEvents::Auto;
  }
  const auto& ui = *StyleUI();
  if (ui.IsInert()) {
    return StylePointerEvents::None;
  }
  return ui.ComputedPointerEvents();
}

StyleUserSelect ComputedStyle::UserSelect() const {
  return StyleUI()->IsInert() ? StyleUserSelect::None
                              : StyleUIReset()->ComputedUserSelect();
}

bool ComputedStyle::IsFixedPosContainingBlockForNonSVGTextFrames() const {
  // NOTE: Any CSS properties that influence the output of this function
  // should return FIXPOS_CB_NON_SVG for will-change.
  if (IsRootElementStyle()) {
    return false;
  }

  const auto& disp = *StyleDisplay();
  if (disp.mWillChange.bits & mozilla::StyleWillChangeBits::FIXPOS_CB_NON_SVG) {
    return true;
  }

  const auto& effects = *StyleEffects();
  return effects.HasFilters() || effects.HasBackdropFilters();
}

bool ComputedStyle::IsFixedPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  // NOTE: Any CSS properties that influence the output of this function
  // should also handle will-change appropriately.
  if (aContextFrame->IsInSVGTextSubtree()) {
    return false;
  }
  if (IsFixedPosContainingBlockForNonSVGTextFrames()) {
    return true;
  }
  const auto& disp = *StyleDisplay();
  if (disp.IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() &&
      aContextFrame->IsFrameOfType(nsIFrame::eSupportsContainLayoutAndPaint)) {
    return true;
  }
  if (disp.IsFixedPosContainingBlockForTransformSupportingFrames() &&
      aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms)) {
    return true;
  }
  return false;
}

bool ComputedStyle::IsAbsPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  if (IsFixedPosContainingBlock(aContextFrame)) {
    return true;
  }
  // NOTE: Any CSS properties that influence the output of this function
  // should also handle will-change appropriately.
  return StyleDisplay()->IsPositionedStyle() &&
         !aContextFrame->IsInSVGTextSubtree();
}

}  // namespace mozilla

#endif  // ComputedStyleInlines_h
