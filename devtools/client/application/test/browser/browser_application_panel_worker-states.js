/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "resources/service-workers/controlled-install.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Check for non-existing service worker");
  const isWorkerListEmpty = !!doc.querySelector(".js-registration-list-empty");
  ok(isWorkerListEmpty, "No Service Worker displayed");

  info("Register a service worker with a controlled install in the page.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.wrappedJSObject.registerServiceWorker();
  });

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length > 0);
  info("Wait until the 'Installing' state is displayed");
  await waitUntil(() => {
    const containers = getWorkerContainers(doc);
    if (containers.length === 0) {
      return false;
    }

    const stateEl = containers[0].querySelector(".js-worker-status");
    return stateEl.textContent.toLowerCase() === "installing";
  });

  info("Allow the service worker to complete installation");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.wrappedJSObject.installServiceWorker();
  });

  info("Wait until the 'running' state is displayed");
  await waitUntil(() => {
    const workerContainer = getWorkerContainers(doc)[0];
    const stateEl = workerContainer.querySelector(".js-worker-status");
    return stateEl.textContent.toLowerCase() === "running";
  });

  info("Unregister the service worker");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const registration = await content.wrappedJSObject.sw;
    registration.unregister();
  });

  info("Wait until the service worker is removed from the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 0);

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
