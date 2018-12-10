/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");
const { remoteClientManager } =
  require("devtools/client/shared/remote-debugging/remote-client-manager");

const { l10n } = require("../modules/l10n");

const {
  debugLocalAddon,
  debugRemoteAddon,
  openTemporaryExtension,
  uninstallAddon,
} = require("../modules/extensions-helper");

const {
  getCurrentClient,
  getCurrentRuntime,
} = require("../modules/runtimes-state-helper");

const {
  DEBUG_TARGETS,
  REQUEST_EXTENSIONS_FAILURE,
  REQUEST_EXTENSIONS_START,
  REQUEST_EXTENSIONS_SUCCESS,
  REQUEST_TABS_FAILURE,
  REQUEST_TABS_START,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_FAILURE,
  REQUEST_WORKERS_START,
  REQUEST_WORKERS_SUCCESS,
  RUNTIMES,
} = require("../constants");

function inspectDebugTarget(type, id) {
  return async (_, getState) => {
    const runtime = getCurrentRuntime(getState().runtimes);
    const { runtimeDetails, type: runtimeType } = runtime;

    switch (type) {
      case DEBUG_TARGETS.TAB: {
        // Open tab debugger in new window.
        if (runtimeType === RUNTIMES.NETWORK || runtimeType === RUNTIMES.USB) {
          // Pass the remote id from the client manager so that about:devtools-toolbox can
          // retrieve the connected client directly.
          const remoteId = remoteClientManager.getRemoteId(runtime.id, runtime.type);
          window.open(`about:devtools-toolbox?type=tab&id=${id}&remoteId=${remoteId}`);
        } else if (runtimeType === RUNTIMES.THIS_FIREFOX) {
          window.open(`about:devtools-toolbox?type=tab&id=${id}`);
        }
        break;
      }
      case DEBUG_TARGETS.EXTENSION: {
        if (runtimeType === RUNTIMES.NETWORK || runtimeType === RUNTIMES.USB) {
          const devtoolsClient = runtimeDetails.clientWrapper.client;
          await debugRemoteAddon(id, devtoolsClient);
        } else if (runtimeType === RUNTIMES.THIS_FIREFOX) {
          debugLocalAddon(id);
        }
        break;
      }
      case DEBUG_TARGETS.WORKER: {
        // Open worker toolbox in new window.
        const devtoolsClient = runtimeDetails.clientWrapper.client;
        const front = devtoolsClient.getActor(id);
        gDevToolsBrowser.openWorkerToolbox(front);
        break;
      }

      default: {
        console.error("Failed to inspect the debug target of " +
                      `type: ${ type } id: ${ id }`);
      }
    }
  };
}

function installTemporaryExtension() {
  const message = l10n.getString("about-debugging-tmp-extension-install-message");
  return async (dispatch, getState) => {
    const file = await openTemporaryExtension(window, message);
    try {
      await AddonManager.installTemporaryAddon(file);
    } catch (e) {
      console.error(e);
    }
  };
}

function pushServiceWorker(actor) {
  return async (_, getState) => {
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      await clientWrapper.request({ to: actor, type: "push" });
    } catch (e) {
      console.error(e);
    }
  };
}

function reloadTemporaryExtension(id) {
  return async (_, getState) => {
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const addonTargetFront = await clientWrapper.getAddon({ id });
      await addonTargetFront.reload();
    } catch (e) {
      console.error(e);
    }
  };
}

function removeTemporaryExtension(id) {
  return async () => {
    try {
      await uninstallAddon(id);
    } catch (e) {
      console.error(e);
    }
  };
}

function requestTabs() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_TABS_START });

    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const { tabs } = await clientWrapper.listTabs({ favicons: true });

      dispatch({ type: REQUEST_TABS_SUCCESS, tabs });
    } catch (e) {
      dispatch({ type: REQUEST_TABS_FAILURE, error: e });
    }
  };
}

function requestExtensions() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_EXTENSIONS_START });

    const runtime = getCurrentRuntime(getState().runtimes);
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const addons = await clientWrapper.listAddons();
      let extensions = addons.filter(a => a.debuggable);

      // Filter out system addons unless the dedicated preference is set to true.
      if (!getState().ui.showSystemAddons) {
        extensions = extensions.filter(e => !e.isSystem);
      }

      if (runtime.type !== RUNTIMES.THIS_FIREFOX) {
        // manifestURL can only be used when debugging local addons, remove this
        // information for the extension data.
        extensions.forEach(extension => {
          extension.manifestURL = null;
        });
      }

      const installedExtensions = extensions.filter(e => !e.temporarilyInstalled);
      const temporaryExtensions = extensions.filter(e => e.temporarilyInstalled);

      dispatch({
        type: REQUEST_EXTENSIONS_SUCCESS,
        installedExtensions,
        temporaryExtensions,
      });
    } catch (e) {
      dispatch({ type: REQUEST_EXTENSIONS_FAILURE, error: e });
    }
  };
}

function requestWorkers() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_WORKERS_START });

    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const {
        otherWorkers,
        serviceWorkers,
        sharedWorkers,
      } = await clientWrapper.listWorkers();

      dispatch({
        type: REQUEST_WORKERS_SUCCESS,
        otherWorkers,
        serviceWorkers,
        sharedWorkers,
      });
    } catch (e) {
      dispatch({ type: REQUEST_WORKERS_FAILURE, error: e });
    }
  };
}

function startServiceWorker(actor) {
  return async (_, getState) => {
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      await clientWrapper.request({ to: actor, type: "start" });
    } catch (e) {
      console.error(e);
    }
  };
}

module.exports = {
  inspectDebugTarget,
  installTemporaryExtension,
  pushServiceWorker,
  reloadTemporaryExtension,
  removeTemporaryExtension,
  requestTabs,
  requestExtensions,
  requestWorkers,
  startServiceWorker,
};
