/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_NETWORK_THROTTLING,
} = require("../actions/index");

const INITIAL_NETWORK_THROTTLING = {
  enabled: false,
  profile: "",
};

let reducers = {

  [CHANGE_NETWORK_THROTTLING](throttling, { enabled, profile }) {
    return {
      enabled,
      profile,
    };
  },

};

module.exports = function (throttling = INITIAL_NETWORK_THROTTLING, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return throttling;
  }
  return reducer(throttling, action);
};
