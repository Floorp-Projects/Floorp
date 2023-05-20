/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function () {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let newTab = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);

  gBrowser.pinTab(newTab);
  gBrowser.selectedTab = newTab;

  openTrustedLinkIn("javascript:var x=0;", "current");
  is(gBrowser.tabs.length, 2, "Should open in current tab");

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  openTrustedLinkIn("http://example.com/1", "current");
  is(gBrowser.tabs.length, 2, "Should open in current tab");

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  openTrustedLinkIn("http://example.org/", "current");
  is(gBrowser.tabs.length, 3, "Should open in new tab");

  await BrowserTestUtils.removeTab(newTab);
  await BrowserTestUtils.removeTab(gBrowser.tabs[1]); // example.org tab
});
