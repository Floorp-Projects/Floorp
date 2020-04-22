/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef include_gfx_ipc_UiCompositorControllerMessageTypes_h
#define include_gfx_ipc_UiCompositorControllerMessageTypes_h

namespace mozilla {
namespace layers {

//
// NOTE: These values are also defined in
// mobile/android/geckoview/src/main/java/org/mozilla/geckoview/GeckoSession.java
// and must be kept in sync. Any new message added here must also be added
// there.
//

// clang-format off
enum UiCompositorControllerMessageTypes {
  FIRST_PAINT                      = 0, // Sent from compositor after first paint
  LAYERS_UPDATED                   = 1, // Sent from the compositor when any layer has been updated
  COMPOSITOR_CONTROLLER_OPEN       = 2, // Compositor controller IPC is open
  IS_COMPOSITOR_CONTROLLER_OPEN    = 3  // Special message sent from controller to query if the compositor controller is open
};
// clang-format on

}  // namespace layers
}  // namespace mozilla

#endif  // include_gfx_ipc_UiCompositorControllerMessageTypes_h
