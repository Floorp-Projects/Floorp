/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_START,
} = require("../constants");
const Actions = require("../actions/index");

function debugTargetListenerMiddleware(state) {
  const onTabsUpdated = () => {
    state.dispatch(Actions.requestTabs());
  };

  return next => action => {
    switch (action.type) {
      case CONNECT_RUNTIME_SUCCESS: {
        action.client.addListener("tabListChanged", onTabsUpdated);
        break;
      }
      case DISCONNECT_RUNTIME_START: {
        state.getState().runtime.client.removeListener("tabListChanged", onTabsUpdated);
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
