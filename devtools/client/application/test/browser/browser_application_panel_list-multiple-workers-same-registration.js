/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WORKER1_URL = URL_ROOT + "resources/service-workers/simple.html";
const WORKER2_URL = URL_ROOT + "resources/service-workers/debug.html";

add_task(async function() {
  await enableApplicationPanel();

  await openTabAndWaitForWorker(WORKER1_URL);
  const { panel, tab, toolbox } = await openTabAndWaitForWorker(WORKER2_URL);

  const doc = panel.panelWin.document;

  let registrationContainer = getWorkerContainers(doc)[0];

  info("Wait until the unregister button is displayed for the registration");
  await waitUntil(() => {
    registrationContainer = getWorkerContainers(doc)[0];
    return registrationContainer.querySelector(".js-unregister-button");
  });

  const scopeEl = registrationContainer.querySelector(".js-sw-scope");
  const expectedScope =
    "example.com/browser/devtools/client/application/test/" +
    "browser/resources/service-workers/";
  ok(
    scopeEl.textContent.startsWith(expectedScope),
    "Registration has the expected scope"
  );

  // check the workers data
  // note that the worker from WORKER2_URL will appear second in the list with
  // the "installed" state
  info("Check the workers data for this registration");
  const workers = registrationContainer.querySelectorAll(".js-sw-worker");
  is(workers.length, 2, "Registration has two workers");
  // check url for worker from WORKER1_URL
  const url1El = workers[0].querySelector(".js-source-url");
  is(url1El.textContent, "empty-sw.js", "First worker has correct URL");
  // check url for worker from WORKER2_URL
  const url2El = workers[1].querySelector(".js-source-url");
  is(url2El.textContent, "debug-sw.js", "Second worker has correct URL");

  await unregisterAllWorkers(toolbox.target.client, doc);

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

async function openTabAndWaitForWorker(url) {
  const { panel, toolbox, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  return { panel, toolbox, tab };
}
