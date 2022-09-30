/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EXTENSION_BGSCRIPT_STATUSES,
  EXTENSION_BGSCRIPT_STATUS_UPDATED,
  UNWATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");

const RootResourceCommand = require("resource://devtools/shared/commands/root-resource/root-resource-command.js");

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

  let rootResourceCommand;

  function onExtensionsBackgroundScriptStatusAvailable(resources) {
    for (const resource of resources) {
      const backgroundScriptStatus = resource.payload.isRunning
        ? EXTENSION_BGSCRIPT_STATUSES.RUNNING
        : EXTENSION_BGSCRIPT_STATUSES.STOPPED;

      store.dispatch({
        type: EXTENSION_BGSCRIPT_STATUS_UPDATED,
        id: resource.payload.addonId,
        backgroundScriptStatus,
      });
    }
  }

  return next => async action => {
    switch (action.type) {
      case WATCH_RUNTIME_SUCCESS: {
        const { runtime } = action;
        const { clientWrapper } = runtime.runtimeDetails;

        rootResourceCommand = clientWrapper.createRootResourceCommand();

        // Watch extensions background script status updates.
        await rootResourceCommand
          .watchResources(
            [RootResourceCommand.TYPES.EXTENSIONS_BGSCRIPT_STATUS],
            { onAvailable: onExtensionsBackgroundScriptStatusAvailable }
          )
          .catch(e => {
            // Log an error if watching this resource rejects (e.g. if
            // the promise was not resolved yet when about:debugging tab
            // or the RDP connection to a remote target has been closed).
            console.error(e);
          });

        // Tabs
        clientWrapper.on("tabListChanged", onTabsUpdated);

        // Addons
        clientWrapper.on("addonListChanged", onExtensionsUpdated);

        // Workers
        clientWrapper.on("workersUpdated", onWorkersUpdated);
        break;
      }
      case UNWATCH_RUNTIME_START: {
        const { runtime } = action;
        const { clientWrapper } = runtime.runtimeDetails;

        // Stop watching extensions background script status updates.
        try {
          rootResourceCommand?.unwatchResources(
            [RootResourceCommand.TYPES.EXTENSIONS_BGSCRIPT_STATUS],
            { onAvailable: onExtensionsBackgroundScriptStatusAvailable }
          );
        } catch (e) {
          // Log an error  if watching this resource rejects (e.g. if
          // the promise was not resolved yet when about:debugging tab
          // or the RDP connection to a remote target has been closed).
          console.error(e);
        }

        // Tabs
        clientWrapper.off("tabListChanged", onTabsUpdated);

        // Addons
        clientWrapper.off("addonListChanged", onExtensionsUpdated);

        // Workers
        clientWrapper.off("workersUpdated", onWorkersUpdated);
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
