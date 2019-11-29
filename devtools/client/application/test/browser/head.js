/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */

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

  // Wait for dom.ipc.processCount to be updated before releasing processes.
  Services.ppmm.releaseCachedProcesses();
}

async function enableApplicationPanel() {
  // Enable all preferences related to service worker debugging.
  await enableServiceWorkerDebugging();

  // Enable application panel in DevTools.
  await pushPref("devtools.application.enabled", true);
}

function getWorkerContainers(doc) {
  return doc.querySelectorAll(".js-sw-container");
}

function navigate(target, url, waitForTargetEvent = "navigate") {
  executeSoon(() => target.navigateTo({ url }));
  return once(target, waitForTargetEvent);
}

async function openNewTabAndApplicationPanel(url) {
  const tab = await addTab(url);
  const target = await TargetFactory.forTab(tab);

  const toolbox = await gDevTools.showToolbox(target, "application");
  const panel = toolbox.getCurrentPanel();
  return { panel, tab, target, toolbox };
}

async function unregisterAllWorkers(client) {
  info("Wait until all workers have a valid registrationFront");
  let workers;
  await asyncWaitUntil(async function() {
    workers = await client.mainRoot.listAllWorkers();
    const allWorkersRegistered = workers.service.every(
      worker => !!worker.registrationFront
    );
    return allWorkersRegistered;
  });

  info("Unregister all service workers");
  for (const worker of workers.service) {
    await worker.registrationFront.unregister();
  }
}

async function waitForWorkerRegistration(swTab) {
  info("Wait until the registration appears on the window");
  const swBrowser = swTab.linkedBrowser;
  await asyncWaitUntil(async () =>
    ContentTask.spawn(swBrowser, {}, function() {
      return content.wrappedJSObject.getRegistration();
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
