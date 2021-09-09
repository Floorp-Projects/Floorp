/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZTestAccess_h
#define mozilla_layers_APZTestAccess_h

#include <cstddef>  // for size_t
#include <cstdint>  // for int32_t

namespace mozilla {
namespace layers {

struct ScrollMetadata;
class WebRenderLayerScrollData;
class WebRenderScrollData;

// The only purpose of this class is to serve as a single type that can be
// the target of a "friend class" declaration in APZ classes that want to
// give APZ test code access to their private members.
// APZ test code can then access those members via this class.
class APZTestAccess {
 public:
  static void InitializeForTest(WebRenderLayerScrollData& aLayer,
                                int32_t aDescendantCount);
  static ScrollMetadata& GetScrollMetadataMut(WebRenderLayerScrollData& aLayer,
                                              WebRenderScrollData& aOwner,
                                              size_t aIndex);
};

}  // namespace layers
}  // namespace mozilla

#endif
