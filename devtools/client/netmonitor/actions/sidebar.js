/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  DISABLE_TOGGLE_BUTTON,
  SHOW_SIDEBAR,
  TOGGLE_SIDEBAR,
} = require("../constants");

/**
 * Change ToggleButton disabled state.
 *
 * @param {boolean} disabled - expected button disabled state
 */
function disableToggleButton(disabled) {
  return {
    type: DISABLE_TOGGLE_BUTTON,
    disabled: disabled,
  };
}

/**
 * Change sidebar visible state.
 *
 * @param {boolean} visible - expected sidebar visible state
 */
function showSidebar(visible) {
  return {
    type: SHOW_SIDEBAR,
    visible: visible,
  };
}

/**
 * Toggle to show/hide sidebar.
 */
function toggleSidebar() {
  return {
    type: TOGGLE_SIDEBAR,
  };
}

module.exports = {
  disableToggleButton,
  showSidebar,
  toggleSidebar,
};
