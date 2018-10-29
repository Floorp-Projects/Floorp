/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from debug-target-pane_collapsibilities_head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Load collapsibilities helpers
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "debug-target-pane_collapsibilities_head.js", this);

// Make sure the ADB addon is removed and ADB is stopped when the test ends.
registerCleanupFunction(async function() {
  try {
    const { adbAddon } = require("devtools/shared/adb/adb-addon");
    await adbAddon.uninstall();
  } catch (e) {
    // Will throw if the addon is already uninstalled, ignore exceptions here.
  }
  const { ADB } = require("devtools/shared/adb/adb");
  await ADB.kill();
});

/**
 * Enable the new about:debugging panel.
 */
async function enableNewAboutDebugging() {
  await pushPref("devtools.aboutdebugging.new-enabled", true);
  await pushPref("devtools.aboutdebugging.network", true);
}

async function openAboutDebugging(page, win) {
  await enableNewAboutDebugging();

  info("opening about:debugging");
  const tab = await addTab("about:debugging", { window: win });
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;
  const { AboutDebugging } = window;

  info("Wait until the main about debugging container is available");
  await waitUntil(() => document.querySelector(".app"));

  info("Wait until the client connection was established");
  await waitUntil(() => document.querySelector(".js-runtime-page"));

  // Wait until the about:debugging target is visible in the tab list
  // Otherwise, we might have a race condition where TAB1 is discovered by the initial
  // listTabs from the watchRuntime action, instead of being discovered after the
  // TAB_UPDATED event. See analysis in Bug 1493968.
  info("Wait until tabs are displayed");
  await waitUntilState(AboutDebugging.store, state => {
    return state.debugTargets.tabs.length > 0;
  });

  info("Wait until pre-installed add-ons are displayed");
  await waitUntilState(AboutDebugging.store, state => {
    return state.debugTargets.installedExtensions.length > 0;
  });

  info("Wait until internal 'other workers' are displayed");
  await waitUntilState(AboutDebugging.store, state => {
    return state.debugTargets.otherWorkers.length > 0;
  });

  return { tab, document, window };
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

  info("Click on the Connect item in the sidebar");
  connectSidebarItem.click();

  info("Wait until Connect page is displayed");
  await waitUntil(() => doc.querySelector(".js-connect-page"));
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
