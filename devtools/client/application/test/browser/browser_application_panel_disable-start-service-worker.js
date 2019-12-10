/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

/**
 * Tests that Start button is disabled for service workers, when they cannot be debugged
 */
add_task(async function() {
  await enableApplicationPanel();

  // Setting a low idle_timeout and idle_extended_timeout will allow the service worker
  // to reach the STOPPED state quickly, which will allow us to test the start button.
  // The default value is 30000 milliseconds.
  info("Set a low service worker idle timeout");
  await pushPref("dom.serviceWorkers.idle_timeout", 1000);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", 1000);

  // disable sw debugging
  info("Disable service worker debugging");
  await pushPref(
    "devtools.debugger.features.windowless-service-workers",
    false
  );

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
