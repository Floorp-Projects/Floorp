/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_DISPLAY_PIXEL_RATIO,
  CHANGE_USER_AGENT,
  TOGGLE_LEFT_ALIGNMENT,
  TOGGLE_RELOAD_ON_TOUCH_SIMULATION,
  TOGGLE_RELOAD_ON_USER_AGENT,
  TOGGLE_TOUCH_SIMULATION,
  TOGGLE_USER_AGENT_INPUT,
} = require("./index");

module.exports = {
  /**
   * The pixel ratio of the display has changed. This may be triggered by the user
   * when changing the monitor resolution, or when the window is dragged to a different
   * display with a different pixel ratio.
   */
  changeDisplayPixelRatio(displayPixelRatio) {
    return {
      type: CHANGE_DISPLAY_PIXEL_RATIO,
      displayPixelRatio,
    };
  },

  changeUserAgent(userAgent) {
    return {
      type: CHANGE_USER_AGENT,
      userAgent,
    };
  },

  toggleLeftAlignment(enabled) {
    return {
      type: TOGGLE_LEFT_ALIGNMENT,
      enabled,
    };
  },

  toggleReloadOnTouchSimulation(enabled) {
    return {
      type: TOGGLE_RELOAD_ON_TOUCH_SIMULATION,
      enabled,
    };
  },

  toggleReloadOnUserAgent(enabled) {
    return {
      type: TOGGLE_RELOAD_ON_USER_AGENT,
      enabled,
    };
  },

  toggleTouchSimulation(enabled) {
    return {
      type: TOGGLE_TOUCH_SIMULATION,
      enabled,
    };
  },

  toggleUserAgentInput(enabled) {
    return {
      type: TOGGLE_USER_AGENT_INPUT,
      enabled,
    };
  },
};
