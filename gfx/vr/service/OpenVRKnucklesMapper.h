/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OPENVRKNUCKLESMAPPER_H
#define GFX_VR_SERVICE_OPENVRKNUCKLESMAPPER_H

#include "OpenVRControllerMapper.h"

namespace mozilla {
namespace gfx {

class OpenVRKnucklesMapper : public OpenVRControllerMapper {
 public:
  OpenVRKnucklesMapper() = default;
  virtual ~OpenVRKnucklesMapper() = default;
  virtual void UpdateButtons(VRControllerState& aControllerState,
                             ControllerInfo& aControllerInfo);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_OPENVRKNUCKLESMAPPER_H
