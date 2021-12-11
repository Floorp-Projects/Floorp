/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const {
  remoteClientManager,
} = require("devtools/client/shared/remote-debugging/remote-client-manager");
const Services = require("Services");

const { l10n } = require("devtools/client/aboutdebugging/src/modules/l10n");

const {
  isSupportedDebugTargetPane,
} = require("devtools/client/aboutdebugging/src/modules/debug-target-support");

const {
  openTemporaryExtension,
  uninstallAddon,
} = require("devtools/client/aboutdebugging/src/modules/extensions-helper");

const {
  getCurrentClient,
  getCurrentRuntime,
} = require("devtools/client/aboutdebugging/src/modules/runtimes-state-helper");

const {
  DEBUG_TARGET_PANE,
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
  TEMPORARY_EXTENSION_RELOAD_FAILURE,
  TEMPORARY_EXTENSION_RELOAD_START,
  TEMPORARY_EXTENSION_RELOAD_SUCCESS,
  RUNTIMES,
} = require("devtools/client/aboutdebugging/src/constants");

const Actions = require("devtools/client/aboutdebugging/src/actions/index");

function getTabForUrl(url) {
  for (const navigator of Services.wm.getEnumerator("navigator:browser")) {
    for (const browser of navigator.gBrowser.browsers) {
      if (
        browser.contentWindow &&
        browser.contentWindow.location.href === url
      ) {
        return navigator.gBrowser.getTabForBrowser(browser);
      }
    }
  }

  return null;
}

function inspectDebugTarget(type, id) {
  return async ({ dispatch, getState }) => {
    const runtime = getCurrentRuntime(getState().runtimes);

    const urlParams = {
      id,
      type,
    };

    if (runtime.id !== RUNTIMES.THIS_FIREFOX) {
      urlParams.remoteId = remoteClientManager.getRemoteId(
        runtime.id,
        runtime.type
      );
    }

    const url = `about:devtools-toolbox?${new window.URLSearchParams(
      urlParams
    )}`;

    const existingTab = getTabForUrl(url);
    if (existingTab) {
      const navigator = existingTab.ownerGlobal;
      navigator.gBrowser.selectedTab = existingTab;
      navigator.focus();
    } else {
      window.open(url);
    }

    dispatch(
      Actions.recordTelemetryEvent("inspect", {
        target_type: type.toUpperCase(),
        runtime_type: runtime.type,
      })
    );
  };
}

function installTemporaryExtension() {
  const message = l10n.getString(
    "about-debugging-tmp-extension-install-message"
  );
  return async ({ dispatch, getState }) => {
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

function pushServiceWorker(id, registrationFront) {
  return async ({ dispatch, getState }) => {
    try {
      // The push button is only available if canDebugServiceWorkers is true.
      // With this configuration, `push` should always be called on the
      // registration front, and not on the (service) WorkerTargetActor.
      await registrationFront.push();
    } catch (e) {
      console.error(e);
    }
  };
}

function reloadTemporaryExtension(id) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: TEMPORARY_EXTENSION_RELOAD_START, id });
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const addonTargetFront = await clientWrapper.getAddon({ id });
      await addonTargetFront.reload();
      dispatch({ type: TEMPORARY_EXTENSION_RELOAD_SUCCESS, id });
    } catch (e) {
      const error = typeof e === "string" ? new Error(e) : e;
      dispatch({ type: TEMPORARY_EXTENSION_RELOAD_FAILURE, id, error });
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
  return async ({ dispatch, getState }) => {
    dispatch({ type: REQUEST_TABS_START });

    const runtime = getCurrentRuntime(getState().runtimes);
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const isSupported = isSupportedDebugTargetPane(
        runtime.runtimeDetails.info.type,
        DEBUG_TARGET_PANE.TAB
      );
      const tabs = isSupported ? await clientWrapper.listTabs() : [];

      // Fetch the favicon for all tabs.
      await Promise.all(
        tabs.map(descriptorFront => descriptorFront.retrieveFavicon())
      );

      dispatch({ type: REQUEST_TABS_SUCCESS, tabs });
    } catch (e) {
      dispatch({ type: REQUEST_TABS_FAILURE, error: e });
    }
  };
}

function requestExtensions() {
  return async ({ dispatch, getState }) => {
    dispatch({ type: REQUEST_EXTENSIONS_START });

    const runtime = getCurrentRuntime(getState().runtimes);
    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const isIconDataURLRequired = runtime.type !== RUNTIMES.THIS_FIREFOX;
      const addons = await clientWrapper.listAddons({
        iconDataURL: isIconDataURLRequired,
      });

      const showHiddenAddons = getState().ui.showHiddenAddons;

      // Filter out non-debuggable addons as well as hidden ones, unless the dedicated
      // preference is set to true.
      const extensions = addons.filter(
        a => a.debuggable && (!a.hidden || showHiddenAddons)
      );

      const installedExtensions = extensions.filter(
        e => !e.temporarilyInstalled
      );
      const temporaryExtensions = extensions.filter(
        e => e.temporarilyInstalled
      );

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
  return async ({ dispatch, getState }) => {
    dispatch({ type: REQUEST_PROCESSES_START });

    const clientWrapper = getCurrentClient(getState().runtimes);

    try {
      const mainProcessDescriptorFront = await clientWrapper.getMainProcess();
      dispatch({
        type: REQUEST_PROCESSES_SUCCESS,
        mainProcess: {
          id: 0,
          processFront: mainProcessDescriptorFront,
        },
      });
    } catch (e) {
      dispatch({ type: REQUEST_PROCESSES_FAILURE, error: e });
    }
  };
}

function requestWorkers() {
  return async ({ dispatch, getState }) => {
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
  return async () => {
    try {
      await registrationFront.start();
    } catch (e) {
      console.error(e);
    }
  };
}

function unregisterServiceWorker(registrationFront) {
  return async () => {
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
