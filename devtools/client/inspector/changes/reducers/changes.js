/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const INITIAL_CHANGES = [];

let reducers = {

};

module.exports = function (changes = INITIAL_CHANGES, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return changes;
  }
  return reducer(changes, action);
};
