/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollableLayerGuid.h"

#include "mozilla/HashFunctions.h"  // for HashGeneric

namespace mozilla {
namespace layers {

ScrollableLayerGuid::ScrollableLayerGuid()
    : mLayersId{0}, mPresShellId(0), mScrollId(0) {}

ScrollableLayerGuid::ScrollableLayerGuid(LayersId aLayersId,
                                         uint32_t aPresShellId,
                                         ViewID aScrollId)
    : mLayersId(aLayersId), mPresShellId(aPresShellId), mScrollId(aScrollId) {}

bool ScrollableLayerGuid::operator==(const ScrollableLayerGuid& other) const {
  return mLayersId == other.mLayersId && mPresShellId == other.mPresShellId &&
         mScrollId == other.mScrollId;
}

bool ScrollableLayerGuid::operator!=(const ScrollableLayerGuid& other) const {
  return !(*this == other);
}

bool ScrollableLayerGuid::operator<(const ScrollableLayerGuid& other) const {
  if (mLayersId < other.mLayersId) {
    return true;
  }
  if (mLayersId == other.mLayersId) {
    if (mPresShellId < other.mPresShellId) {
      return true;
    }
    if (mPresShellId == other.mPresShellId) {
      return mScrollId < other.mScrollId;
    }
  }
  return false;
}

std::size_t ScrollableLayerGuid::HashFn::operator()(
    const ScrollableLayerGuid& aGuid) const {
  return HashGeneric(uint64_t(aGuid.mLayersId), aGuid.mPresShellId,
                     aGuid.mScrollId);
}

std::size_t ScrollableLayerGuid::HashIgnoringPresShellFn::operator()(
    const ScrollableLayerGuid& aGuid) const {
  return HashGeneric(uint64_t(aGuid.mLayersId), aGuid.mScrollId);
}

bool ScrollableLayerGuid::EqualIgnoringPresShellFn::operator()(
    const ScrollableLayerGuid& lhs, const ScrollableLayerGuid& rhs) const {
  return lhs.mLayersId == rhs.mLayersId && lhs.mScrollId == rhs.mScrollId;
}

}  // namespace layers
}  // namespace mozilla
