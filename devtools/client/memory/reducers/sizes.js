/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions } = require("../constants");
const { immutableUpdate } = require("devtools/shared/DevToolsUtils");

const handlers = Object.create(null);

handlers[actions.RESIZE_SHORTEST_PATHS] = function (sizes, { size }) {
  return immutableUpdate(sizes, { shortestPathsSize: size });
};

module.exports = function (sizes = { shortestPathsSize: .5 }, action) {
  const handler = handlers[action.type];
  return handler ? handler(sizes, action) : sizes;
};
