/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_SIDEBAR_SIZE } = require("../actions/index");

const INITIAL_SIZE = {
  width: 0,
  height: 0
};

const reducers = {
  [UPDATE_SIDEBAR_SIZE](_, { size }) {
    return size;
  }
};

module.exports = function (size = INITIAL_SIZE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(size, action) : size;
};
