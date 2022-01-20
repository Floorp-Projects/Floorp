/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  OPEN_ACTION_BAR,
  SELECT_ACTION_BAR_TAB,
  PANELS,
} = require("devtools/client/netmonitor/src/constants");

/**
 * Open the entire HTTP Custom Request panel
 * @returns {Function}
 */
function openHTTPCustomRequest() {
  return ({ dispatch, getState }) => {
    dispatch({ type: OPEN_ACTION_BAR, open: true });
  };
}

/**
 * Toggle visibility of New Custom Request panel in network panel
 */
function toggleHTTPCustomRequestPanel() {
  return ({ dispatch, getState }) => {
    const state = getState();

    const shouldClose =
      state.ui.networkActionOpen &&
      state.ui.selectedActionBarTabId === PANELS.HTTP_CUSTOM_REQUEST;
    dispatch({ type: OPEN_ACTION_BAR, open: !shouldClose });

    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.HTTP_CUSTOM_REQUEST,
    });
  };
}

module.exports = {
  openHTTPCustomRequest,
  toggleHTTPCustomRequestPanel,
};
