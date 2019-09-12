/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel only displays service workers from the
 * current domain.
 */

const SIMPLE_URL = URL_ROOT + "resources/service-workers/simple.html";
const OTHER_URL = SIMPLE_URL.replace("example.com", "test1.example.com");
const EMPTY_URL = (URL_ROOT + "resources/service-workers/empty.html").replace(
  "example.com",
  "test2.example.com"
);

add_task(async function() {
  await enableApplicationPanel();

  const { panel, target } = await openNewTabAndApplicationPanel(SIMPLE_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  let scopeEl = getWorkerContainers(doc)[0].querySelector(".js-sw-scope");
  ok(
    scopeEl.textContent.startsWith("example.com"),
    "First service worker registration is displayed for the correct domain"
  );

  info(
    "Navigate to another page for a different domain with no service worker"
  );

  await navigate(target, EMPTY_URL);
  info("Wait until the service worker list is updated");
  await waitUntil(() => doc.querySelector(".worker-list-empty") !== null);
  ok(
    true,
    "No service workers are shown for an empty page in a different domain."
  );

  info(
    "Navigate to another page for a different domain with another service worker"
  );
  await navigate(target, OTHER_URL);

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  scopeEl = getWorkerContainers(doc)[0].querySelector(".js-sw-scope");
  ok(
    scopeEl.textContent.startsWith("test1.example.com"),
    "Second service worker registration is displayed for the correct domain"
  );

  await unregisterAllWorkers(target.client);
});
