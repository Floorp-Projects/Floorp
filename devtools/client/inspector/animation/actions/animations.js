/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_ANIMATIONS,
  UPDATE_DETAIL_VISIBILITY,
  UPDATE_ELEMENT_PICKER_ENABLED,
  UPDATE_SELECTED_ANIMATION,
  UPDATE_SIDEBAR_SIZE
} = require("./index");

module.exports = {
  /**
   * Update the list of animation in the animation inspector.
   */
  updateAnimations(animations) {
    return {
      type: UPDATE_ANIMATIONS,
      animations,
    };
  },

  /**
   * Update visibility of detail pane.
   */
  updateDetailVisibility(detailVisibility) {
    return {
      type: UPDATE_DETAIL_VISIBILITY,
      detailVisibility,
    };
  },

  /**
   * Update the state of element picker in animation inspector.
   */
  updateElementPickerEnabled(elementPickerEnabled) {
    return {
      type: UPDATE_ELEMENT_PICKER_ENABLED,
      elementPickerEnabled,
    };
  },

  /**
   * Update selected animation.
   */
  updateSelectedAnimation(selectedAnimation) {
    return {
      type: UPDATE_SELECTED_ANIMATION,
      selectedAnimation,
    };
  },

  /**
   * Update the sidebar size.
   */
  updateSidebarSize(sidebarSize) {
    return {
      type: UPDATE_SIDEBAR_SIZE,
      sidebarSize,
    };
  }
};
