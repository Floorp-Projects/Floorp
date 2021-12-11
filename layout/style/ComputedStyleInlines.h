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
#include "nsStyleStruct.h"

namespace mozilla {

namespace detail {
template <typename T, const T* (ComputedStyle::*Method)() const>
void TriggerImageLoads(dom::Document& aDocument, const ComputedStyle* aOldStyle,
                       ComputedStyle* aStyle) {
  if constexpr (T::kHasTriggerImageLoads) {
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
  auto& ui = *StyleUI();
  if (ui.IsInert()) {
    return StylePointerEvents::None;
  }
  return ui.ComputedPointerEvents();
}

StyleUserSelect ComputedStyle::UserSelect() const {
  return StyleUI()->IsInert() ? StyleUserSelect::None
                              : StyleUIReset()->ComputedUserSelect();
}

}  // namespace mozilla

#endif  // ComputedStyleInlines_h
