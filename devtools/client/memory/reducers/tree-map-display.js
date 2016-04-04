/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { actions, treeMapDisplays } = require("../constants");
const DEFAULT_TREE_MAP_DISPLAY = treeMapDisplays.coarseType;

const handlers = Object.create(null);

handlers[actions.SET_TREE_MAP_DISPLAY] = function (_, { display }) {
  return display;
};

module.exports = function (state = DEFAULT_TREE_MAP_DISPLAY, action) {
  const handler = handlers[action.type];
  return handler ? handler(state, action) : state;
};
