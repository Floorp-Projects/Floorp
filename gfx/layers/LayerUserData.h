/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERUSERDATA_H
#define GFX_LAYERUSERDATA_H

namespace mozilla {
namespace layers {

/**
 * Base class for userdata objects attached to layers and layer managers.
 *
 * We define it here in a separate header so clients only need to include
 * this header for their class definitions, rather than pulling in Layers.h.
 * Everything else in Layers.h should be forward-declarable.
 */
class LayerUserData {
public:
  virtual ~LayerUserData() {}
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_LAYERUSERDATA_H */
