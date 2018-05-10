"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.InitialState = InitialState;
exports.default = update;
exports.getExpandedState = getExpandedState;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Source tree reducer
 * @module reducers/source-tree
 */
function InitialState() {
  return (0, _makeRecord2.default)({
    expanded: null
  })();
}

function update(state = InitialState(), action) {
  switch (action.type) {
    case "SET_EXPANDED_STATE":
      return state.set("expanded", action.expanded);
  }

  return state;
}

function getExpandedState(state) {
  return state.sourceTree.get("expanded");
}