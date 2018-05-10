"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.fetchScopes = fetchScopes;

var _selectors = require("../../selectors/index");

var _mapScopes = require("./mapScopes");

var _promise = require("../utils/middleware/promise");

var _extra = require("./extra");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function fetchScopes() {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    const frame = (0, _selectors.getSelectedFrame)(getState());

    if (!frame || (0, _selectors.getGeneratedFrameScope)(getState(), frame.id)) {
      return;
    }

    const scopes = dispatch({
      type: "ADD_SCOPES",
      frame,
      [_promise.PROMISE]: client.getFrameScopes(frame)
    });
    await dispatch((0, _extra.fetchExtra)());
    await dispatch((0, _mapScopes.mapScopes)(scopes, frame));
  };
}