/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_NETWORK_THROTTLING,
} = require("./actions");

const INITIAL_STATE = {
  enabled: false,
  profile: "",
};

function throttlingReducer(state = INITIAL_STATE, action) {
  switch (action.type) {
    case CHANGE_NETWORK_THROTTLING: {
      return {
        enabled: action.enabled,
        profile: action.profile
      };
    }
    default:
      return state;
  }
}

module.exports = throttlingReducer;
