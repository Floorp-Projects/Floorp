/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef include_gfx_ipc_UiCompositorControllerMessageTypes_h
#define include_gfx_ipc_UiCompositorControllerMessageTypes_h

namespace mozilla {
namespace layers {

//
// NOTE: These values are also defined in mobile/android/geckoview/src/main/java/org/mozilla/gecko/gfx/LayerView.java
//       and must be kept in sync. Any new message added here must also be added there to the AnimatorMessageType enum.
//

enum UiCompositorControllerMessageTypes {
  STATIC_TOOLBAR_NEEDS_UPDATE      = 0,  // Sent from compositor when the static toolbar wants to hide.
  STATIC_TOOLBAR_READY             = 1,  // Sent from compositor when the static toolbar image has been updated and is ready to animate.
  TOOLBAR_HIDDEN                   = 2,  // Sent to compositor when the real toolbar has been hidden.
  TOOLBAR_VISIBLE                  = 3,  // Sent to compositor when the real toolbar has been made  visible
  TOOLBAR_SHOW                     = 4,  // Sent from compositor when the real toolbar should be shown
  FIRST_PAINT                      = 5,  // Sent from compositor after first paint
  REQUEST_SHOW_TOOLBAR_IMMEDIATELY = 6,  // Sent to the compositor when the snapshot should be shown immediately
  REQUEST_SHOW_TOOLBAR_ANIMATED    = 7,  // Sent to the compositor when the snapshot should be shown with an animation
  REQUEST_HIDE_TOOLBAR_IMMEDIATELY = 8,  // Sent to the compositor when the snapshot should be hidden immediately
  REQUEST_HIDE_TOOLBAR_ANIMATED    = 9,  // Sent to the compositor when the snapshot should be hidden with an animation
  LAYERS_UPDATED                   = 10, // Sent from the compositor when any layer has been updated
  TOOLBAR_SNAPSHOT_FAILED          = 11, // Sent to compositor when the toolbar snapshot fails.
  COMPOSITOR_CONTROLLER_OPEN       = 20, // Compositor controller IPC is open
  IS_COMPOSITOR_CONTROLLER_OPEN    = 21  // Special message sent from controller to query if the compositor controller is open
};

} // namespace layers
} // namespace mozilla

#endif // include_gfx_ipc_UiCompositorControllerMessageTypes_h
