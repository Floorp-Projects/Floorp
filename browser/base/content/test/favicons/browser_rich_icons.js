/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

add_task(async function test_richIcons() {
  const URL = ROOT + "file_rich_icon.html";
  const EXPECTED_ICON = ROOT + "moz.png";
  const EXPECTED_RICH_ICON = ROOT + "rich_moz_2.png";

  let tabPromises = Promise.all([
    waitForFaviconMessage(true, EXPECTED_ICON),
    waitForFaviconMessage(false, EXPECTED_RICH_ICON),
  ]);

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let [tabIcon, richIcon] = await tabPromises;

  is(richIcon.iconURL, EXPECTED_RICH_ICON, "should choose the largest rich icon");
  is(tabIcon.iconURL, EXPECTED_ICON, "should use the non-rich icon for the tab");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_maskIcons() {
  const URL = ROOT + "file_mask_icon.html";
  const EXPECTED_ICON = "http://mochi.test:8888/favicon.ico";

  let promise = waitForFaviconMessage(true, EXPECTED_ICON);
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let tabIcon = await promise;
  is(tabIcon.iconURL, EXPECTED_ICON, "should ignore the mask icons and load the root favicon");

  BrowserTestUtils.removeTab(tab);
});
