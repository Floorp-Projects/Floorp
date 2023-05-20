/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel fetches a manifest when in the Manifest Page
 */

add_task(async function () {
  info("Test that manifest page has a link that opens the manifest JSON file");
  const url = URL_ROOT_SSL + "resources/manifest/load-ok-manifest-link.html";
  const manifestUrl = URL_ROOT_SSL + "resources/manifest/manifest.json";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest JSON link");
  await waitUntil(() => doc.querySelector(".js-manifest-json-link") !== null);
  ok(true, "Link to JSON is displayed");

  info("Click on link and wait till the JSON is opened in a new tab");
  // click on the link and wait for the new tab to open
  const onTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    `${manifestUrl}`
  );
  const link = doc.querySelector(".js-manifest-json-link");
  link.click();
  const jsonTab = await onTabLoaded;
  ok(jsonTab, "The manifest JSON was opened in a new tab");

  // close the tabs
  info("Closing the page tab.");
  await BrowserTestUtils.removeTab(tab);
  info("Closing the manifest JSON tab.");
  await BrowserTestUtils.removeTab(jsonTab);
});

add_task(async function () {
  info(
    "Test that manifest page does not show a link for manifests embedded in a data url"
  );
  const url = URL_ROOT_SSL + "resources/manifest/load-ok.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to load");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest loaded successfully");
  is(
    doc.querySelector(".js-manifest-json-link"),
    null,
    "No JSON link is shown"
  );

  // close tab
  info("Closing the tab");
  await BrowserTestUtils.removeTab(tab);
});
