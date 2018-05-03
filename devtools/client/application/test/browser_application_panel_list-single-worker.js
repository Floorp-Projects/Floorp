/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = URL_ROOT + "service-workers/dynamic-registration.html";

add_task(async function() {
  await enableApplicationPanel();

  let { panel, tab } = await openNewTabAndApplicationPanel(TAB_URL);
  let doc = panel.panelWin.document;

  let isWorkerListEmpty = !!doc.querySelector(".worker-list-empty");
  ok(isWorkerListEmpty, "No Service Worker displayed");

  info("Register a service worker in the page.");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.registerServiceWorker();
  });

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length > 0);

  let workerContainer = getWorkerContainers(doc)[0];

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => workerContainer.querySelector(".unregister-button"));

  let scopeEl = workerContainer.querySelector(".service-worker-scope");
  let expectedScope = "example.com/browser/devtools/client/application/test/" +
                      "service-workers/";
  ok(scopeEl.textContent.startsWith(expectedScope),
    "Service worker has the expected scope");

  info("Unregister the service worker");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    let registration = await content.wrappedJSObject.sw;
    registration.unregister();
  });

  info("Wait until the service worker is removed from the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 0);
});

