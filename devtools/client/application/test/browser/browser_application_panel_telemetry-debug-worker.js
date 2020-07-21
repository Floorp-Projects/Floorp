/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/simple.html";

// check telemetry for debugging a service worker
add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab, toolbox, target } = await openNewTabAndApplicationPanel(
    TAB_URL
  );

  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");
  setupTelemetryTest();

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  const container = getWorkerContainers(doc)[0];
  info("Wait until the debug button is displayed and enabled");
  await waitUntil(() => {
    const button = container.querySelector(".js-debug-button");
    return button && !button.disabled;
  });

  info("Click on the debug button and wait for debugger to be ready");
  const debugButton = container.querySelector(".js-debug-button");
  debugButton.click();
  await waitUntil(() => toolbox.getPanel("jsdebugger"));

  const events = getTelemetryEvents("jsdebugger");
  const openToolboxEvent = events.find(event => event.method == "enter");
  ok(openToolboxEvent.session_id > 0, "Event has a valid session id");
  is(
    openToolboxEvent.start_state,
    "application",
    "Event has the 'application' start state"
  );

  // clean up and close the tab
  await unregisterAllWorkers(target.client, doc);
  info("Closing the tab.");
  await target.client.waitForRequestsToSettle();
  await BrowserTestUtils.removeTab(tab);
});
