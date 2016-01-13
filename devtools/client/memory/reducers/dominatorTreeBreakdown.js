/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { actions, dominatorTreeBreakdowns } = require("../constants");
const DEFAULT_BREAKDOWN = dominatorTreeBreakdowns.coarseType.breakdown;

const handlers = Object.create(null);

handlers[actions.SET_DOMINATOR_TREE_BREAKDOWN] = function (_, { breakdown }) {
  return breakdown;
};

module.exports = function (state = DEFAULT_BREAKDOWN, action) {
  const handler = handlers[action.type];
  return handler ? handler(state, action) : state;
};
