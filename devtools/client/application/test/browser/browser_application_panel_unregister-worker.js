/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab, target } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  const workerContainer = getWorkerContainers(doc)[0];

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => workerContainer.querySelector(".js-unregister-button"));
  info("Click the unregister button");
  const button = workerContainer.querySelector(".js-unregister-button");
  button.click();
  info("Wait until the service worker is removed from the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 0);
  ok(true, "Service worker list is empty");

  // just in case cleanup
  await unregisterAllWorkers(target.client);
  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
