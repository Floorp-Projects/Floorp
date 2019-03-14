/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

/* import-globals-from helper-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-mocks.js", this);

// Make sure the ADB addon is removed and ADB is stopped when the test ends.
registerCleanupFunction(async function() {
  try {
    const { adbAddon } = require("devtools/shared/adb/adb-addon");
    await adbAddon.uninstall();
  } catch (e) {
    // Will throw if the addon is already uninstalled, ignore exceptions here.
  }
  const { adbProcess } = require("devtools/shared/adb/adb-process");
  await adbProcess.kill();

  const { remoteClientManager } =
    require("devtools/client/shared/remote-debugging/remote-client-manager");
  await remoteClientManager.removeAllClients();
});

/**
 * Enable the new about:debugging panel.
 */
async function enableNewAboutDebugging() {
  await pushPref("devtools.aboutdebugging.new-enabled", true);
  await pushPref("devtools.aboutdebugging.network", true);
}

async function openAboutDebugging({ enableWorkerUpdates } = {}) {
  if (!enableWorkerUpdates) {
    silenceWorkerUpdates();
  }

  await enableNewAboutDebugging();

  info("opening about:debugging");

  const tab = await addTab("about:debugging");
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".js-connect-page"));

  return { tab, document, window };
}

async function openAboutDevtoolsToolbox(doc, tab, win) {
  info("Open about:devtools-toolbox page");
  const target = findDebugTargetByText("about:debugging", doc);
  ok(target, "about:debugging tab target appeared");
  const inspectButton = target.querySelector(".js-debug-target-inspect-button");
  ok(inspectButton, "Inspect button for about:debugging appeared");
  inspectButton.click();
  await Promise.all([
    waitUntil(() => tab.nextElementSibling),
    waitForRequestsToSettle(win.AboutDebugging.store),
    gDevTools.once("toolbox-ready"),
  ]);

  info("Wait for about:devtools-toolbox tab will be selected");
  const devtoolsTab = tab.nextElementSibling;
  await waitUntil(() => gBrowser.selectedTab === devtoolsTab);
  const devtoolsBrowser = gBrowser.selectedBrowser;

  return {
    devtoolsBrowser,
    devtoolsDocument: devtoolsBrowser.contentDocument,
    devtoolsTab,
    devtoolsWindow: devtoolsBrowser.contentWindow,
  };
}

async function closeAboutDevtoolsToolbox(aboutDebuggingDocument, devtoolsTab, win) {
  info("Close about:devtools-toolbox page");
  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");
  await removeTab(devtoolsTab);
  await onToolboxDestroyed;
  // Changing the tab will also trigger a request to list tabs, so wait until the selected
  // tab has changed to wait for requests to settle.
  await waitUntil(() => gBrowser.selectedTab !== devtoolsTab);

  // Wait for removing about:devtools-toolbox tab info from about:debugging.
  await waitUntil(() =>
    !findDebugTargetByText("about:devtools-toolbox?", aboutDebuggingDocument));

  await waitForRequestsToSettle(win.AboutDebugging.store);
}

async function reloadAboutDebugging(tab) {
  info("reload about:debugging");

  await refreshTab(tab);
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;
  info("wait for the initial about:debugging requests to be successful");
  await waitForRequestsSuccess(window.AboutDebugging.store);

  return document;
}

// Wait for all about:debugging target request actions to succeed.
// They will typically be triggered after watching a new runtime or loading
// about:debugging.
function waitForRequestsSuccess(store) {
  return Promise.all([
    waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
    waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
    waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
  ]);
}

/**
 * Wait for all client requests to settle, meaning here that no new request has been
 * dispatched after the provided delay.
 */
async function waitForRequestsToSettle(store, delay = 500) {
  let hasSettled = false;

  // After each iteration of this while loop, we check is the timerPromise had the time
  // to resolve or if we captured a REQUEST_*_SUCCESS action before.
  while (!hasSettled) {
    let timer;

    // This timer will be executed only if no REQUEST_*_SUCCESS action is dispatched
    // during the delay. We consider that when no request are received for some time, it
    // means there are no ongoing requests anymore.
    const timerPromise = new Promise(resolve => {
      timer = setTimeout(() => {
        hasSettled = true;
        resolve();
      }, delay);
    });

    // Wait either for a REQUEST_*_SUCCESS to be dispatched, or for the timer to resolve.
    await Promise.race([
      waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
      waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
      waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
      timerPromise,
    ]);

    // Clear the timer to avoid setting hasSettled to true accidently unless timerPromise
    // was the first to resolve.
    clearTimeout(timer);
  }
}

function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

/**
 * Navigate to "This Firefox"
 */
async function selectThisFirefoxPage(doc, store) {
  info("Select This Firefox page");
  const onRequestSuccess = waitForRequestsSuccess(store);
  doc.location.hash = "#/runtime/this-firefox";
  info("Wait for requests to be complete");
  await onRequestSuccess;
  info("Wait for runtime page to be rendered");
  await waitUntil(() => doc.querySelector(".js-runtime-page"));
}

/**
 * Navigate to the Connect page. Resolves when the Connect page is rendered.
 */
async function selectConnectPage(doc) {
  const sidebarItems = doc.querySelectorAll(".js-sidebar-item");
  const connectSidebarItem = [...sidebarItems].find(element => {
    return element.textContent === "Connect";
  });
  ok(connectSidebarItem, "Sidebar contains a Connect item");
  const connectLink = connectSidebarItem.querySelector(".js-sidebar-link");
  ok(connectLink, "Sidebar contains a Connect link");

  info("Click on the Connect link in the sidebar");
  connectLink.click();

  info("Wait until Connect page is displayed");
  await waitUntil(() => doc.querySelector(".js-connect-page"));
}

function getDebugTargetPane(title, document) {
  // removes the suffix "(<NUMBER>)" in debug target pane's title, if needed
  const sanitizeTitle = (x) => {
    return x.replace(/\s+\(\d+\)$/, "");
  };

  const targetTitle = sanitizeTitle(title);
  for (const titleEl of document.querySelectorAll(".js-debug-target-pane-title")) {
    if (sanitizeTitle(titleEl.textContent) !== targetTitle) {
      continue;
    }

    return titleEl.closest(".js-debug-target-pane");
  }

  return null;
}

function findDebugTargetByText(text, document) {
  const targets = [...document.querySelectorAll(".js-debug-target-item")];
  return targets.find(target => target.textContent.includes(text));
}

function findSidebarItemByText(text, document) {
  const sidebarItems = document.querySelectorAll(".js-sidebar-item");
  return [...sidebarItems].find(element => {
    return element.textContent.includes(text);
  });
}

function findSidebarItemLinkByText(text, document) {
  const links = document.querySelectorAll(".js-sidebar-link");
  return [...links].find(element => {
    return element.textContent.includes(text);
  });
}

async function connectToRuntime(deviceName, document) {
  info(`Wait until the sidebar item for ${deviceName} appears`);
  await waitUntil(() => findSidebarItemByText(deviceName, document));
  const sidebarItem = findSidebarItemByText(deviceName, document);
  const connectButton = sidebarItem.querySelector(".js-connect-button");
  ok(connectButton, `Connect button is displayed for the runtime ${deviceName}`);

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  await waitUntil(() => !sidebarItem.querySelector(".js-connect-button"));
}

async function selectRuntime(deviceName, name, document) {
  const sidebarItem = findSidebarItemByText(deviceName, document);
  sidebarItem.querySelector(".js-sidebar-link").click();

  await waitUntil(() => {
    const runtimeInfo = document.querySelector(".js-runtime-name");
    return runtimeInfo && runtimeInfo.textContent.includes(name);
  });
}

function getToolbox(win) {
  return gDevTools.getToolboxes().find(toolbox => toolbox.win === win);
}

/**
 * Open the performance profiler dialog. Assumes the client is a mocked remote runtime
 * client.
 */
async function openProfilerDialog(client, doc) {
  const onProfilerLoaded = new Promise(r => {
    client.loadPerformanceProfiler = r;
  });

  info("Click on the Profile Runtime button");
  const profileButton = doc.querySelector(".js-profile-runtime-button");
  profileButton.click();

  info("Wait for the loadPerformanceProfiler callback to be executed on client-wrapper");
  return onProfilerLoaded;
}
