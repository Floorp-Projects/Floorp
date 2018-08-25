/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_START,
} = require("../constants");
const Actions = require("../actions/index");

function debugTargetListenerMiddleware(store) {
  const onExtensionsUpdated = () => {
    store.dispatch(Actions.requestExtensions());
  };

  const onTabsUpdated = () => {
    store.dispatch(Actions.requestTabs());
  };

  const extensionsListener = {
    onDisabled() {
      onExtensionsUpdated();
    },

    onEnabled() {
      onExtensionsUpdated();
    },

    onInstalled() {
      onExtensionsUpdated();
    },

    onOperationCancelled() {
      onExtensionsUpdated();
    },

    onUninstalled() {
      onExtensionsUpdated();
    },

    onUninstalling() {
      onExtensionsUpdated();
    },
  };

  const onWorkersUpdated = () => {
    store.dispatch(Actions.requestWorkers());
  };

  return next => action => {
    switch (action.type) {
      case CONNECT_RUNTIME_SUCCESS: {
        const { client } = action;
        client.addListener("tabListChanged", onTabsUpdated);
        AddonManager.addAddonListener(extensionsListener);
        client.addListener("workerListChanged", onWorkersUpdated);
        client.addListener("serviceWorkerRegistrationListChanged", onWorkersUpdated);
        client.addListener("processListChanged", onWorkersUpdated);
        client.addListener("registration-changed", onWorkersUpdated);
        client.addListener("push-subscription-modified", onWorkersUpdated);
        break;
      }
      case DISCONNECT_RUNTIME_START: {
        const { client } = store.getState().runtime;
        client.removeListener("tabListChanged", onTabsUpdated);
        AddonManager.removeAddonListener(extensionsListener);
        client.removeListener("workerListChanged", onWorkersUpdated);
        client.removeListener("serviceWorkerRegistrationListChanged", onWorkersUpdated);
        client.removeListener("processListChanged", onWorkersUpdated);
        client.removeListener("registration-changed", onWorkersUpdated);
        client.removeListener("push-subscription-modified", onWorkersUpdated);
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
