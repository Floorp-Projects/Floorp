/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

// check telemetry for starting a service worker
add_task(async function () {
  info("Set a low service worker idle timeout");
  await pushPref("dom.serviceWorkers.idle_timeout", 1000);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", 1000);

  await enableApplicationPanel();

  const { panel, tab, commands } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");
  await waitForWorkerRegistration(tab);

  setupTelemetryTest();

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  info("Wait until the start button is displayed and enabled");
  const container = getWorkerContainers(doc)[0];
  await waitUntil(() => {
    const button = container.querySelector(".js-start-button");
    return button && !button.disabled;
  });

  info("Click the start button");
  const button = container.querySelector(".js-start-button");
  button.click();

  checkTelemetryEvent({ method: "start_worker" });

  // clean up and close the tab
  await unregisterAllWorkers(commands.client, doc);
  info("Closing the tab.");
  await commands.client.waitForRequestsToSettle();
  await BrowserTestUtils.removeTab(tab);
});
