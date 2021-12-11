/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel can display several service workers applying to the
 * same domain.
 */

const SIMPLE_URL = URL_ROOT + "resources/service-workers/simple.html";
const OTHER_SCOPE_URL = URL_ROOT + "resources/service-workers/scope-page.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, commands, tab } = await openNewTabAndApplicationPanel(
    SIMPLE_URL
  );
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() =>
    getWorkerContainers(doc)[0].querySelector(".js-unregister-button")
  );

  ok(true, "First service worker registration is displayed");

  info(
    "Navigate to another page for the same domain with another service worker"
  );
  await navigateTo(OTHER_SCOPE_URL);

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 2);

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() =>
    getWorkerContainers(doc)[1].querySelector(".js-unregister-button")
  );

  ok(true, "Second service worker registration is displayed");

  await unregisterAllWorkers(commands.client, doc);

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
