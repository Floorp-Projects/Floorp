/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  OPEN_SIDEBAR,
  TOGGLE_SIDEBAR,
} = require("../constants");

/**
 * Change sidebar open state.
 *
 * @param {boolean} open - open state
 */
function openSidebar(open) {
  return {
    type: OPEN_SIDEBAR,
    open,
  };
}

/**
 * Toggle sidebar open state.
 */
function toggleSidebar() {
  return {
    type: TOGGLE_SIDEBAR,
  };
}

module.exports = {
  openSidebar,
  toggleSidebar,
};
