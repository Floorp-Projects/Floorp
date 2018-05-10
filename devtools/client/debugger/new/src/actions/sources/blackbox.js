"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.toggleBlackBox = toggleBlackBox;

var _promise = require("../utils/middleware/promise");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */
function toggleBlackBox(source) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const {
      isBlackBoxed,
      id
    } = source;
    return dispatch({
      type: "BLACKBOX",
      source,
      [_promise.PROMISE]: client.blackBox(id, isBlackBoxed)
    });
  };
}