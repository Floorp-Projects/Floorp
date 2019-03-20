/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");
const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");
const { Toolbox } = require("devtools/client/framework/toolbox");
const { remoteClientManager } =
  require("devtools/client/shared/remote-debugging/remote-client-manager");

const { l10n } = require("../modules/l10n");

const {
  debugAddon,
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
  REQUEST_PROCESSES_FAILURE,
  REQUEST_PROCESSES_START,
  REQUEST_PROCESSES_SUCCESS,
  REQUEST_TABS_FAILURE,
  REQUEST_TABS_START,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_FAILURE,
  REQUEST_WORKERS_START,
  REQUEST_WORKERS_SUCCESS,
  TEMPORARY_EXTENSION_INSTALL_FAILURE,
  TEMPORARY_EXTENSION_INSTALL_START,
  TEMPORARY_EXTENSION_INSTALL_SUCCESS,
  RUNTIMES,
} = require("../constants");

const Actions = require("./index");

function inspectDebugTarget(type, id) {
  return async (dispatch, getState) => {
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
        await debugAddon(id, runtimeDetails.clientWrapper.client);
        break;
      }
      case DEBUG_TARGETS.PROCESS: {
        const devtoolsClient = runtimeDetails.clientWrapper.client;
        const processTargetFront = devtoolsClient.getActor(id);
        const toolbox = await gDevTools.showToolbox(processTargetFront, null,
          Toolbox.HostType.WINDOW);

        // Once the target is destroyed after closing the toolbox, the front is gone and
        // can no longer be used. Extensions don't have the issue because we don't list
        // the target fronts directly, but proxies on which we call connect to create a
        // target front when we want to inspect them. Local workers don't have this issue
        // because closing the toolbox stops several workers which will indirectly trigger
        // a requestWorkers action. However workers on a remote runtime have the exact
        // same issue.
        // To workaround the issue we request processes after the toolbox is closed.
        toolbox.once("destroy", () => dispatch(Actions.requestProcesses()));
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

    dispatch(Actions.recordTelemetryEvent("inspect", {
      "target_type": type,
      "runtime_type": runtimeType,
    }));
  };
}

function installTemporaryExtension() {
  const message = l10n.getString("about-debugging-tmp-extension-install-message");
  return async (dispatch, getState) => {
    dispatch({ type: TEMPORARY_EXTENSION_INSTALL_START });
    const file = await openTemporaryExtension(window, message);
    try {
      await AddonManager.installTemporaryAddon(file);
      dispatch({ type: TEMPORARY_EXTENSION_INSTALL_SUCCESS });
    } catch (e) {
      dispatch({ type: TEMPORARY_EXTENSION_INSTALL_FAILURE, error: e });
    }
  };
}

function pushServiceWorker(id) {
  return async (_, getState) => {
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const workerActor = await clientWrapper.getServiceWorkerFront({ id });
      await workerActor.push();
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
      const tabs = await clientWrapper.listTabs({ favicons: true });

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
      const isIconDataURLRequired = runtime.type !== RUNTIMES.THIS_FIREFOX;
      const addons =
        await clientWrapper.listAddons({ iconDataURL: isIconDataURLRequired });
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

function requestProcesses() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_PROCESSES_START });

    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const mainProcessFront = await clientWrapper.getMainProcess();

      dispatch({
        type: REQUEST_PROCESSES_SUCCESS,
        mainProcess: {
          id: 0,
          processFront: mainProcessFront,
        },
      });
    } catch (e) {
      dispatch({ type: REQUEST_PROCESSES_FAILURE, error: e });
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

      for (const serviceWorker of serviceWorkers) {
        const { registrationFront } = serviceWorker;
        if (!registrationFront) {
          continue;
        }

        const subscription = await registrationFront.getPushSubscription();
        serviceWorker.subscription = subscription;
      }

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

function startServiceWorker(registrationFront) {
  return async (_, getState) => {
    try {
      await registrationFront.start();
    } catch (e) {
      console.error(e);
    }
  };
}

function unregisterServiceWorker(registrationFront) {
  return async (_, getState) => {
    try {
      await registrationFront.unregister();
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
  requestProcesses,
  requestWorkers,
  startServiceWorker,
  unregisterServiceWorker,
};
