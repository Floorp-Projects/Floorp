/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

add_task(async function test_richIcons() {
  const URL = ROOT + "file_rich_icon.html";
  const EXPECTED_ICON = ROOT + "moz.png";
  const EXPECTED_RICH_ICON = ROOT + "rich_moz_2.png";
  // One regular icon and one rich icon. Note that ContentLinkHandler will
  // choose the best rich icon if there are multiple candidates available
  // in the page.
  const EXPECTED_ICON_LOADS = 2;
  let loadCount = 0;
  let tabIconUri;
  let richIconUri;

  const promiseMessage = new Promise(resolve => {
    const mm = window.messageManager;
    mm.addMessageListener("Link:SetIcon", function listenForSetIcon(msg) {
      // Ignore the chrome favicon sets on the tab before the actual page load.
      if (msg.data.url === "chrome://branding/content/icon32.png")
        return;

      if (!msg.data.canUseForTab)
        richIconUri = msg.data.url;

      if (++loadCount === EXPECTED_ICON_LOADS) {
        mm.removeMessageListener("Link:SetIcon", listenForSetIcon);
        resolve();
      }
    });
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await promiseMessage;

  is(richIconUri, EXPECTED_RICH_ICON, "should choose the largest rich icon");

  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly.
  await BrowserTestUtils.waitForCondition(() => {
    tabIconUri = gBrowser.getIcon();
    return tabIconUri != null;
  }, "wait for icon load to finish", 100, 20);
  is(tabIconUri, EXPECTED_ICON, "should use the non-rich icon for the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_maskIcons() {
  const URL = ROOT + "file_mask_icon.html";
  // First we need to clear the failed favicons cache, since previous tests
  // likely added this non-existing icon, and useDefaultIcon would skip it.
  PlacesUtils.favicons.removeFailedFavicon(makeURI("http://mochi.test:8888/favicon.ico"));
  const EXPECTED_ICON = "http://mochi.test:8888/favicon.ico";
  let tabIconUri;

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  await BrowserTestUtils.waitForCondition(() => {
    tabIconUri = gBrowser.getIcon();
    return tabIconUri != null;
  }, "wait for icon load to finish", 100, 20);
  is(tabIconUri, EXPECTED_ICON, "should ignore the mask icons and load the root favicon");
  BrowserTestUtils.removeTab(tab);
});
