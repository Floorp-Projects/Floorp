"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.toggleSkipPausing = toggleSkipPausing;

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * @memberof actions/pause
 * @static
 */
function toggleSkipPausing() {
  return async ({
    dispatch,
    client,
    getState,
    sourceMaps
  }) => {
    const skipPausing = !(0, _selectors.getSkipPausing)(getState()); // NOTE: enable this when we land the endpoint in m-c
    // await client.setSkipPausing(skipPausing);

    dispatch({
      type: "TOGGLE_SKIP_PAUSING",
      skipPausing
    });
  };
}