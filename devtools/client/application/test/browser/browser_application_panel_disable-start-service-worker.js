/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

/**
 * Tests that Start button is disabled for service workers, when they cannot be debugged
 */
add_task(async function() {
  await enableApplicationPanel();

  // disable sw debugging by increasing the # of processes and thus multi-e10s kicking in
  info("Disable service worker debugging");
  await pushPref("dom.ipc.processCount", 8);

  const { panel, tab, target } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  await waitForWorkerRegistration(tab);

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  info("Wait until the start button is displayed");
  const container = getWorkerContainers(doc)[0];
  await waitUntil(() => container.querySelector(".js-start-button"));
  ok(
    container.querySelector(".js-start-button").disabled,
    "Start button is disabled"
  );

  await unregisterAllWorkers(target.client);
});
