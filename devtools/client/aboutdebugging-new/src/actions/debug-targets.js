/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");

const {
  debugLocalAddon,
  debugRemoteAddon,
  getAddonForm,
  openTemporaryExtension,
  uninstallAddon,
} = require("../modules/extensions-helper");

const {
  getCurrentClient,
  getCurrentRuntime
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
    const runtimeType = runtime.type;
    const client = runtime.client;

    switch (type) {
      case DEBUG_TARGETS.TAB: {
        // Open tab debugger in new window.
        if (runtime.type === RUNTIMES.NETWORK) {
          const [host, port] = runtime.id.split(":");
          window.open(`about:devtools-toolbox?type=tab&id=${id}` +
                      `&host=${host}&port=${port}`);
        } else if (runtimeType === RUNTIMES.THIS_FIREFOX) {
          window.open(`about:devtools-toolbox?type=tab&id=${id}`);
        }
        break;
      }
      case DEBUG_TARGETS.EXTENSION: {
        if (runtimeType === RUNTIMES.NETWORK) {
          const addonForm = await getAddonForm(id, client);
          debugRemoteAddon(addonForm, client);
        } else if (runtimeType === RUNTIMES.THIS_FIREFOX) {
          debugLocalAddon(id);
        }
        break;
      }
      case DEBUG_TARGETS.WORKER: {
        // Open worker toolbox in new window.
        gDevToolsBrowser.openWorkerToolbox(client, id);
        break;
      }

      default: {
        console.error("Failed to inspect the debug target of " +
                      `type: ${ type } id: ${ id }`);
      }
    }
  };
}

function installTemporaryExtension(message) {
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
    const client = getCurrentClient(getState().runtimes);

    try {
      await client.request({ to: actor, type: "push" });
    } catch (e) {
      console.error(e);
    }
  };
}

function reloadTemporaryExtension(actor) {
  return async (_, getState) => {
    const client = getCurrentClient(getState().runtimes);

    try {
      await client.request({ to: actor, type: "reload" });
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

    const client = getCurrentClient(getState().runtimes);

    try {
      const { tabs } = await client.listTabs({ favicons: true });

      dispatch({ type: REQUEST_TABS_SUCCESS, tabs });
    } catch (e) {
      dispatch({ type: REQUEST_TABS_FAILURE, error: e.message });
    }
  };
}

function requestExtensions() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_EXTENSIONS_START });

    const client = getCurrentClient(getState().runtimes);

    try {
      const { addons } = await client.listAddons();
      const extensions = addons.filter(a => a.debuggable);
      const installedExtensions = extensions.filter(e => !e.temporarilyInstalled);
      const temporaryExtensions = extensions.filter(e => e.temporarilyInstalled);

      dispatch({
        type: REQUEST_EXTENSIONS_SUCCESS,
        installedExtensions,
        temporaryExtensions,
      });
    } catch (e) {
      dispatch({ type: REQUEST_EXTENSIONS_FAILURE, error: e.message });
    }
  };
}

function requestWorkers() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_WORKERS_START });

    const client = getCurrentClient(getState().runtimes);

    try {
      const {
        other: otherWorkers,
        service: serviceWorkers,
        shared: sharedWorkers,
      } = await client.mainRoot.listAllWorkers();

      dispatch({
        type: REQUEST_WORKERS_SUCCESS,
        otherWorkers,
        serviceWorkers,
        sharedWorkers,
      });
    } catch (e) {
      dispatch({ type: REQUEST_WORKERS_FAILURE, error: e.message });
    }
  };
}

function startServiceWorker(actor) {
  return async (_, getState) => {
    const client = getCurrentClient(getState().runtimes);

    try {
      await client.request({ to: actor, type: "start" });
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
