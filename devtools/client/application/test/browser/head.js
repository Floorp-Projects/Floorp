/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

/**
 * Set all preferences needed to enable service worker debugging and testing.
 */
async function enableServiceWorkerDebugging() {
  // Enable service workers.
  await pushPref("dom.serviceWorkers.enabled", true);
  // Accept workers from mochitest's http.
  await pushPref("dom.serviceWorkers.testing.enabled", true);
  // Force single content process, see Bug 1231208 for the SW refactor that should enable
  // SW debugging in multi-e10s.
  await pushPref("dom.ipc.processCount", 1);

  // Enable service workers in the debugger
  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  // Disable randomly spawning processes during tests
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  // Wait for dom.ipc.processCount to be updated before releasing processes.
  Services.ppmm.releaseCachedProcesses();
}

async function enableApplicationPanel() {
  // FIXME bug 1575427 this rejection is very common.
  const { PromiseTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/PromiseTestUtils.sys.mjs"
  );
  PromiseTestUtils.allowMatchingRejectionsGlobally(
    /this._frontCreationListeners is null/
  );

  // Enable all preferences related to service worker debugging.
  await enableServiceWorkerDebugging();

  // Enable web manifest processing.
  Services.prefs.setBoolPref("dom.manifest.enabled", true);

  // Enable application panel in DevTools.
  await pushPref("devtools.application.enabled", true);
}

function setupTelemetryTest() {
  // Reset all the counts
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");
}

function getTelemetryEvents(objectName) {
  // read the requested events only
  const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  // filter and transform the event data so the relevant info is in a single object:
  // { method: "...", extraField: "...", anotherExtraField: "...", ... }
  const events = snapshot.parent
    .filter(event => event[1] === "devtools.main" && event[3] === objectName)
    .map(event => ({ method: event[2], ...event[5] }));

  return events;
}

function checkTelemetryEvent(expectedEvent, objectName = "application") {
  info("Check telemetry event");
  const events = getTelemetryEvents(objectName);

  // assert we only got 1 event with a valid session ID
  is(events.length, 1, "There was only 1 event logged");
  const [event] = events;
  Assert.greater(
    Number(event.session_id),
    0,
    "There is a valid session_id in the event"
  );

  // assert expected data
  Assert.deepEqual(event, { ...expectedEvent, session_id: event.session_id });
}

function getWorkerContainers(doc) {
  return doc.querySelectorAll(".js-sw-container");
}

async function openNewTabAndApplicationPanel(url) {
  const tab = await addTab(url);

  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "application",
  });
  const panel = toolbox.getCurrentPanel();
  const target = toolbox.target;
  const commands = toolbox.commands;
  return { panel, tab, target, toolbox, commands };
}

async function unregisterAllWorkers(client, doc) {
  // This method is declared in shared-head.js
  await unregisterAllServiceWorkers(client);

  info("Wait for service workers to disappear from the UI");
  waitUntil(() => getWorkerContainers(doc).length === 0);
}

async function waitForWorkerRegistration(swTab) {
  info("Wait until the registration appears on the window");
  const swBrowser = swTab.linkedBrowser;
  await asyncWaitUntil(async () =>
    SpecialPowers.spawn(swBrowser, [], function () {
      return !!content.wrappedJSObject.getRegistration();
    })
  );
}

function selectPage(panel, page) {
  /**
   * Select a page by simulating a user click in the sidebar.
   * @param {string} page The page we want to select (see `PAGE_TYPES`)
   **/
  info(`Selecting application page: ${page}`);
  const doc = panel.panelWin.document;
  const navItem = doc.querySelector(`.js-sidebar-${page}`);
  navItem.click();
}
