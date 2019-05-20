/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYERSTATE_H_
#define LAYERSTATE_H_

namespace mozilla {

enum class LayerState : uint8_t {
  LAYER_NONE,
  LAYER_INACTIVE,
  LAYER_ACTIVE,
  // Force an active layer even if it causes incorrect rendering, e.g.
  // when the layer has rounded rect clips.
  LAYER_ACTIVE_FORCE,
  // Special layer that is metadata only.
  LAYER_ACTIVE_EMPTY,
  // Inactive style layer for rendering SVG effects.
  LAYER_SVG_EFFECTS
};

}  // namespace mozilla

#endif
