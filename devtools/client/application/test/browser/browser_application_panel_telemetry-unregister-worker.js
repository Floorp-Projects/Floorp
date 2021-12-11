/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

// check telemetry for unregistering a service worker
add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab, commands } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  setupTelemetryTest();

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  const workerContainer = getWorkerContainers(doc)[0];

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => workerContainer.querySelector(".js-unregister-button"));
  info("Click the unregister button");
  const button = workerContainer.querySelector(".js-unregister-button");
  button.click();

  checkTelemetryEvent({ method: "unregister_worker" });

  // clean up and close the tab
  await unregisterAllWorkers(commands.client, doc);
  info("Closing the tab.");
  await commands.client.waitForRequestsToSettle();
  await BrowserTestUtils.removeTab(tab);
});
