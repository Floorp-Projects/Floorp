/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testBlankTabReusedAboutAddons() {
  await BrowserTestUtils.withNewTab({ gBrowser }, async browser => {
    let tabCount = gBrowser.tabs.length;
    is(browser, gBrowser.selectedBrowser, "New tab is selected");

    // Opening about:addons shouldn't change the selected tab.
    BrowserAddonUI.openAddonsMgr();

    is(browser, gBrowser.selectedBrowser, "No new tab was opened");

    // Wait for about:addons to load.
    await BrowserTestUtils.browserLoaded(browser);

    is(
      browser.currentURI.spec,
      "about:addons",
      "about:addons should load into blank tab."
    );

    is(gBrowser.tabs.length, tabCount, "Still the same number of tabs");
  });
});
