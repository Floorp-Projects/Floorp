/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UNWATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");
const Actions = require("../actions/index");

function debugTargetListenerMiddleware(store) {
  const onExtensionsUpdated = () => {
    store.dispatch(Actions.requestExtensions());
  };

  const onTabsUpdated = () => {
    store.dispatch(Actions.requestTabs());
  };

  const onWorkersUpdated = () => {
    store.dispatch(Actions.requestWorkers());
  };

  return next => action => {
    switch (action.type) {
      case WATCH_RUNTIME_SUCCESS: {
        const { runtime } = action;
        const { clientWrapper } = runtime.runtimeDetails;

        // Tabs
        clientWrapper.addListener("tabListChanged", onTabsUpdated);

        // Addons
        clientWrapper.addListener("addonListChanged", onExtensionsUpdated);

        // Workers
        clientWrapper.addListener("workersUpdated", onWorkersUpdated);
        break;
      }
      case UNWATCH_RUNTIME_START: {
        const { runtime } = action;
        const { clientWrapper } = runtime.runtimeDetails;

        // Tabs
        clientWrapper.removeListener("tabListChanged", onTabsUpdated);

        // Addons
        clientWrapper.removeListener("addonListChanged", onExtensionsUpdated);

        // Workers
        clientWrapper.removeListener("workersUpdated", onWorkersUpdated);
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
