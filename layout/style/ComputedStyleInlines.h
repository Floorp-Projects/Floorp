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
#include "mozilla/ServoComputedDataInlines.h"
#include "mozilla/ServoUtils.h"
#include "nsPresContext.h"

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

}  // namespace mozilla

#endif  // ComputedStyleInlines_h
