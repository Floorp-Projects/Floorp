/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { actions, dominatorTreeDisplays } = require("../constants");
const DEFAULT_DOMINATOR_TREE_DISPLAY = dominatorTreeDisplays.coarseType;

const handlers = Object.create(null);

handlers[actions.SET_DOMINATOR_TREE_DISPLAY] = function (_, { display }) {
  return display;
};

module.exports = function (state = DEFAULT_DOMINATOR_TREE_DISPLAY, action) {
  const handler = handlers[action.type];
  return handler ? handler(state, action) : state;
};
