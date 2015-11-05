/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let newTab = gBrowser.addTab("http://example.com");
  waitForExplicitFinish();
  BrowserTestUtils.browserLoaded(newTab.linkedBrowser).then(mainPart);

  function mainPart() {
    gBrowser.pinTab(newTab);
    gBrowser.selectedTab = newTab;

    openUILinkIn("http://example.org/", "current", { inBackground: true });
    isnot(gBrowser.selectedTab, newTab, "shouldn't load in background");

    gBrowser.removeTab(newTab);
    gBrowser.removeTab(gBrowser.tabs[1]); // example.org tab
    finish();
  }
}
