/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTypes.h"

#include "mozilla/ipc/ByteBuf.h"
#include "nsStyleConsts.h"

namespace mozilla {
namespace wr {

WindowId NewWindowId() {
  static uint64_t sNextId = 1;

  WindowId id;
  id.mHandle = sNextId++;
  return id;
}

BorderStyle ToBorderStyle(StyleBorderStyle aStyle) {
  switch (aStyle) {
    case StyleBorderStyle::None:
      return wr::BorderStyle::None;
    case StyleBorderStyle::Solid:
      return wr::BorderStyle::Solid;
    case StyleBorderStyle::Double:
      return wr::BorderStyle::Double;
    case StyleBorderStyle::Dotted:
      return wr::BorderStyle::Dotted;
    case StyleBorderStyle::Dashed:
      return wr::BorderStyle::Dashed;
    case StyleBorderStyle::Hidden:
      return wr::BorderStyle::Hidden;
    case StyleBorderStyle::Groove:
      return wr::BorderStyle::Groove;
    case StyleBorderStyle::Ridge:
      return wr::BorderStyle::Ridge;
    case StyleBorderStyle::Inset:
      return wr::BorderStyle::Inset;
    case StyleBorderStyle::Outset:
      return wr::BorderStyle::Outset;
  }
  return wr::BorderStyle::None;
}

wr::RepeatMode ToRepeatMode(StyleBorderImageRepeat aRepeat) {
  switch (aRepeat) {
    case StyleBorderImageRepeat::Stretch:
      return wr::RepeatMode::Stretch;
    case StyleBorderImageRepeat::Repeat:
      return wr::RepeatMode::Repeat;
    case StyleBorderImageRepeat::Round:
      return wr::RepeatMode::Round;
    case StyleBorderImageRepeat::Space:
      return wr::RepeatMode::Space;
    default:
      MOZ_ASSERT(false);
  }

  return wr::RepeatMode::Stretch;
}

ImageRendering ToImageRendering(StyleImageRendering aImageRendering) {
  switch (aImageRendering) {
    case StyleImageRendering::Auto:
    case StyleImageRendering::Smooth:
    case StyleImageRendering::Optimizequality:
      return wr::ImageRendering::Auto;
    case StyleImageRendering::CrispEdges:
      // FIXME(bug 1728831): Historically we've returned Pixelated here, but
      // this should arguably pass CrispEdges to WebRender?
      // return wr::ImageRendering::CrispEdges;
      [[fallthrough]];
    case StyleImageRendering::Optimizespeed:
    case StyleImageRendering::Pixelated:
      return wr::ImageRendering::Pixelated;
  }
  return wr::ImageRendering::Auto;
}

void Assign_WrVecU8(wr::WrVecU8& aVec, mozilla::ipc::ByteBuf&& aOther) {
  aVec.data = aOther.mData;
  aVec.length = aOther.mLen;
  aVec.capacity = aOther.mCapacity;
  aOther.mData = nullptr;
  aOther.mLen = 0;
  aOther.mCapacity = 0;
}

WrSpatialId RootScrollNode() { return wr_root_scroll_node_id(); }

WrSpaceAndClipChain RootScrollNodeWithChain() {
  WrSpaceAndClipChain sacc;
  sacc.clip_chain = wr::ROOT_CLIP_CHAIN;
  sacc.space = wr_root_scroll_node_id();
  return sacc;
}

WrSpaceAndClipChain InvalidScrollNodeWithChain() {
  WrSpaceAndClipChain sacc;
  sacc.clip_chain = std::numeric_limits<uint64_t>::max();
  sacc.space = wr_root_scroll_node_id();
  return sacc;
}

}  // namespace wr
}  // namespace mozilla
