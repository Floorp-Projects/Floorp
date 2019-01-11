/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGETS,
  UNWATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");
const Actions = require("../actions/index");
const { isSupportedDebugTarget } = require("../modules/debug-target-support");

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

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.TAB)) {
          clientWrapper.addListener("tabListChanged", onTabsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.EXTENSION)) {
          clientWrapper.addListener("addonListChanged", onExtensionsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.WORKER)) {
          clientWrapper.addListener("workerListChanged", onWorkersUpdated);
          clientWrapper.onFront("contentProcessTarget", front => {
            clientWrapper.contentProcessFronts.push(front);
            front.on("workerListChanged", onWorkersUpdated);
          });

          clientWrapper.onFront("serviceWorkerRegistration", front => {
            clientWrapper.serviceWorkerRegistrationFronts.push(front);
            front.on("push-subscription-modified", onWorkersUpdated);
            front.on("registration-changed", onWorkersUpdated);
          });

          clientWrapper.addListener("serviceWorkerRegistrationListChanged",
            onWorkersUpdated);
          clientWrapper.addListener("processListChanged", onWorkersUpdated);
        }
        break;
      }
      case UNWATCH_RUNTIME_START: {
        const { runtime } = action;
        const { clientWrapper } = runtime.runtimeDetails;

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.TAB)) {
          clientWrapper.removeListener("tabListChanged", onTabsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.EXTENSION)) {
          clientWrapper.removeListener("addonListChanged", onExtensionsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.WORKER)) {
          clientWrapper.removeListener("workerListChanged", onWorkersUpdated);
          clientWrapper.removeListener("serviceWorkerRegistrationListChanged",
            onWorkersUpdated);

          for (const front of clientWrapper.contentProcessFronts) {
            front.off("workerListChanged", onWorkersUpdated);
          }
          clientWrapper.contentProcessFronts = [];

          for (const front of clientWrapper.serviceWorkerRegistrationFronts) {
            front.off("push-subscription-modified", onWorkersUpdated);
            front.off("registration-changed", onWorkersUpdated);
          }
          clientWrapper.serviceWorkerRegistrationFronts = [];

          clientWrapper.removeListener("processListChanged", onWorkersUpdated);
        }
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
