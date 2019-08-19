/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel fetches a manifest when in the Manifest Page
 */

add_task(async function() {
  info("Test that manifest page loads the manifest successfully");
  const url = URL_ROOT + "resources/manifest/load-ok.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to load");
  await waitUntil(() => doc.querySelector(".js-manifest-loaded-ok") !== null);
  ok(true, "Manifest loaded successfully");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Test that manifest page shows an error when failing to load");
  const url = URL_ROOT + "resources/manifest/load-fail.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to fail to load");
  await waitUntil(
    () => doc.querySelector(".js-manifest-loaded-error") !== null
  );
  ok(true, "Manifest page displays loading error");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Test that manifest page shows a message when there is no manifest");
  const url = URL_ROOT + "resources/manifest/load-no-manifest.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the 'no manifest' message to appear");
  await waitUntil(
    () => doc.querySelector(".js-manifest-non-existing") !== null
  );
  ok(true, "Manifest page displays a 'no manifest' message");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
