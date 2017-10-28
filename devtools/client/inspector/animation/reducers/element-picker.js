/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_ELEMENT_PICKER_ENABLED } = require("../actions/index");

const INITIAL_STATE = { isEnabled: false };

const reducers = {
  [UPDATE_ELEMENT_PICKER_ENABLED](state, { isEnabled }) {
    return Object.assign({}, state, {
      isEnabled
    });
  }
};

module.exports = function (state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
