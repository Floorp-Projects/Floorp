/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel can display several service workers applying to the
 * same domain.
 */

const SIMPLE_URL = URL_ROOT + "service-workers/simple.html";
const OTHER_SCOPE_URL = URL_ROOT + "service-workers/scope-page.html";

add_task(async function() {
  await enableApplicationPanel();

  let { panel, target } = await openNewTabAndApplicationPanel(SIMPLE_URL);
  let doc = panel.panelWin.document;

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => getWorkerContainers(doc)[0]
    .querySelector(".js-unregister-button"));

  ok(true, "First service worker registration is displayed");

  info("Navigate to another page for the same domain with another service worker");
  await navigate(target, OTHER_SCOPE_URL);

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 2);

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => getWorkerContainers(doc)[1]
    .querySelector(".js-unregister-button"));

  ok(true, "Second service worker registration is displayed");

  await unregisterAllWorkers(target.client);
});
