/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { actions, breakdowns } = require("../constants");
const DEFAULT_BREAKDOWN = breakdowns.coarseType.breakdown;

let handlers = Object.create(null);

handlers[actions.SET_BREAKDOWN] = function (_, action) {
  return Object.assign({}, action.breakdown);
};

module.exports = function (state=DEFAULT_BREAKDOWN, action) {
  let handle = handlers[action.type];
  if (handle) {
    return handle(state, action);
  }
  return state;
};
