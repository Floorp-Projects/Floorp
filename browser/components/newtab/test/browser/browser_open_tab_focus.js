/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_open_tab_focus() {
  await setTestTopSites();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );
  // Specially wait for potentially preloaded browsers
  let browser = tab.linkedBrowser;
  await waitForPreloaded(browser);
  // Wait for React to render something
  await SpecialPowers.spawn(browser, [], async () => {
    await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".top-sites-list .top-site-button .title")
    );
  });

  await BrowserTestUtils.synthesizeMouse(
    `.top-sites-list .top-site-button .title`,
    2,
    2,
    { accelKey: true },
    browser
  );

  Assert.strictEqual(
    gBrowser.selectedTab,
    tab,
    "The original tab is still the selected tab"
  );
  BrowserTestUtils.removeTab(gBrowser.tabs[2]); // example.org tab
  BrowserTestUtils.removeTab(tab); // The original tab
});
