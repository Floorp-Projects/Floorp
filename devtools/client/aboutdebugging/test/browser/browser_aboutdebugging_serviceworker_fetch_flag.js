/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);

const FETCH_SW_JS = URL_ROOT + "resources/service-workers/fetch-sw.js";
const FETCH_SW_HTML = URL_ROOT + "resources/service-workers/fetch-sw.html";

const EMPTY_SW_JS = URL_ROOT + "resources/service-workers/empty-sw.js";
const EMPTY_SW_HTML = URL_ROOT + "resources/service-workers/empty-sw.html";

/**
 * Test that the appropriate fetch flag is displayed for service workers.
 */
add_task(async function() {
  await enableServiceWorkerDebugging();
  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Test fetch status for a service worker listening to fetch events");
  await testServiceWorkerFetchStatus(
    document,
    FETCH_SW_HTML,
    FETCH_SW_JS,
    true
  );

  info("Test fetch status for a service worker not listening to fetch events");
  await testServiceWorkerFetchStatus(
    document,
    EMPTY_SW_HTML,
    EMPTY_SW_JS,
    false
  );

  await removeTab(tab);
});

async function testServiceWorkerFetchStatus(doc, url, workerUrl, isListening) {
  // Open a tab that registers a fetch service worker.
  const swTab = await addTab(url);

  info("Wait until the service worker appears and is running");
  const targetElement = await waitForServiceWorkerRunning(workerUrl, doc);

  const expectedClassName = isListening
    ? ".qa-worker-fetch-listening"
    : ".qa-worker-fetch-not-listening";
  const fetchStatus = targetElement.querySelector(expectedClassName);
  ok(!!fetchStatus, "Found the expected fetch status: " + expectedClassName);

  info("Unregister the service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(workerUrl, doc));

  info("Remove the service worker tab");
  await removeTab(swTab);
}
