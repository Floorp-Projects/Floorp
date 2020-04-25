/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test switching for the top-level target.

// We use about:robots, because this page will run in the parent process.
// Navigating from about:robots to a regular content page will always trigger
// a target switch, with or without fission.
const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI_WORKERS =
  URL_ROOT + "resources/service-workers/simple.html";
const CONTENT_PROCESS_URI_MANIFEST =
  URL_ROOT + "resources/manifest/load-ok.html";

// test workers when target switching
add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);
  await enableApplicationPanel();

  info("Open a page that runs in the parent process");
  const { panel, toolbox, tab } = await openNewTabAndApplicationPanel(
    PARENT_PROCESS_URI
  );
  const doc = panel.panelWin.document;

  info("Check for non-existing service worker");
  selectPage(panel, "service-workers");
  const isWorkerListEmpty = !!doc.querySelector(".js-registration-list-empty");
  ok(isWorkerListEmpty, "No Service Worker displayed");

  info("Navigate to a page that runs in the child process");
  await navigateTo(CONTENT_PROCESS_URI_WORKERS);

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  // close the tab
  info("Closing the tab.");
  await unregisterAllWorkers(toolbox.target.client, doc);
  await BrowserTestUtils.removeTab(tab);
});

// test manifest when target switching
add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);
  await enableApplicationPanel();

  info("Open a page that runs in the parent process");
  const { panel, tab } = await openNewTabAndApplicationPanel(
    PARENT_PROCESS_URI
  );
  const doc = panel.panelWin.document;

  info("Waiting for the 'no manifest' message to appear");
  selectPage(panel, "manifest");
  await waitUntil(() => doc.querySelector(".js-manifest-empty") !== null);

  info("Navigate to a page that runs in the child process");
  await navigateTo(CONTENT_PROCESS_URI_MANIFEST);

  info("Waiting for the manifest to load");
  selectPage(panel, "manifest");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest loaded successfully");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
