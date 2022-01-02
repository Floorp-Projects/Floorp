/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel refetches the page manifest when reloading
 * or navigating to a new page
 */

add_task(async function() {
  await enableApplicationPanel();

  info("Loading a page with no manifest");
  let url = URL_ROOT + "resources/manifest/load-no-manifest.html";
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the 'no manifest' message to appear");
  await waitFor(() => doc.querySelector(".js-manifest-empty") !== null);
  ok(true, "Manifest page displays a 'no manifest' message");

  info("Navigating to a page with a manifest");
  url = URL_ROOT + "resources/manifest/load-ok.html";
  await navigateTo(url);

  info("Waiting for the manifest to show up");
  await waitFor(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest displayed successfully");

  info("Navigating to a page with a manifest that fails to load");
  url = URL_ROOT + "resources/manifest/load-fail.html";
  await navigateTo(url);

  info("Waiting for the manifest to fail to load");
  await waitFor(() => doc.querySelector(".js-manifest-loaded-error") !== null);
  ok(true, "Manifest page displays loading error");

  info("Reloading");
  await navigateTo(url);

  info("Waiting for the manifest to fail to load");
  await waitFor(() => doc.querySelector(".js-manifest-loaded-error") !== null);
  ok(true, "Manifest page displays loading error");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
