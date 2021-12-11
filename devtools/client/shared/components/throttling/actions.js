/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  CHANGE_NETWORK_THROTTLING: "CHANGE_NETWORK_THROTTLING",
};

function changeNetworkThrottling(enabled, profile) {
  return {
    type: actionTypes.CHANGE_NETWORK_THROTTLING,
    enabled,
    profile,
  };
}

module.exports = {
  ...actionTypes,
  changeNetworkThrottling,
};
