/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the manifest is being properly shown
 */

add_task(async function() {
  info("Test that we are displaying correctly the sidebar");

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel();
  const doc = panel.panelWin.document;

  info("Waiting for the sidebar to be displayed");
  await waitUntil(() => doc.querySelector(".js-sidebar") !== null);
  ok(true, "Sidebar is being displayed");

  await waitUntil(() => doc.querySelector(".js-manifest-page") !== null);
  ok(true, "Manifest page was loaded per default.");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Test that we are displaying correctly the selected page - manifest");

  await enableApplicationPanel();
  const { panel, tab, target } = await openNewTabAndApplicationPanel();
  const doc = panel.panelWin.document;

  info("Select service worker page");
  selectPage(panel, "service-workers");
  await waitUntil(() => doc.querySelector(".js-service-workers-page") !== null);

  info("Select manifest page in the sidebar");
  const link = doc.querySelector(".js-sidebar-manifest");
  link.click();

  await waitUntil(() => doc.querySelector(".js-manifest-page") !== null);
  ok(true, "Manifest page was selected.");

  await unregisterAllWorkers(target.client);

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info(
    "Test that we are displaying correctly the selected page - service workers"
  );
  const url = URL_ROOT + "resources/manifest/load-ok.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to load");
  await waitUntil(() => doc.querySelector(".js-manifest-page") !== null);
  ok(true, "Manifest page was selected.");

  info("Select service worker page in the sidebar");
  const link = doc.querySelector(".js-sidebar-service-workers");
  link.click();

  await waitUntil(() => doc.querySelector(".js-service-workers-page") !== null);
  ok(true, "Service workers page was selected.");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
