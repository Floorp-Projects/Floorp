/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_NETWORK_THROTTLING,
} = require("devtools/client/shared/components/throttling/actions");

/**
 * Network throttling middleware is responsible for
 * updating/syncing currently connected backend
 * according to user actions.
 */
function throttlingMiddleware(connector) {
  return store => next => action => {
    const res = next(action);
    if (action.type === CHANGE_NETWORK_THROTTLING) {
      connector.updateNetworkThrottling(action.enabled, action.profile);
    }
    return res;
  };
}

module.exports = throttlingMiddleware;
