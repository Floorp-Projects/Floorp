/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "https://example.com/";

add_task(async function closing_last_tab_should_not_switch_to_fx_view() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.closeWindowWithLastTab", false]],
  });
  info("Opening window...");
  const win = await BrowserTestUtils.openNewBrowserWindow({
    waitForTabURL: "about:newtab",
  });
  const firstTab = win.gBrowser.selectedTab;
  info("Opening Firefox View tab...");
  await openFirefoxViewTab(win);
  info("Switch back to new tab...");
  await BrowserTestUtils.switchTab(win.gBrowser, firstTab);
  info("Load web page in new tab...");
  const loaded = BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    URL
  );
  BrowserTestUtils.startLoadingURIString(win.gBrowser.selectedBrowser, URL);
  await loaded;
  info("Opening new browser tab...");
  const secondTab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    URL
  );
  info("Close all broswer tabs...");
  await BrowserTestUtils.removeTab(firstTab);
  await BrowserTestUtils.removeTab(secondTab);
  isnot(
    win.gBrowser.selectedTab,
    win.FirefoxViewHandler.tab,
    "The selected tab should not be the Firefox View tab"
  );
  await BrowserTestUtils.closeWindow(win);
});
