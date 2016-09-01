/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let newTab = gBrowser.addTab();
  waitForExplicitFinish();
  BrowserTestUtils.browserLoaded(newTab.linkedBrowser).then(mainPart);

  function mainPart() {
    gBrowser.pinTab(newTab);
    gBrowser.selectedTab = newTab;

    openUILinkIn("javascript:var x=0;", "current");
    is(gBrowser.tabs.length, 2, "Should open in current tab");

    openUILinkIn("http://example.com/1", "current");
    is(gBrowser.tabs.length, 2, "Should open in current tab");

    openUILinkIn("http://example.org/", "current");
    is(gBrowser.tabs.length, 3, "Should open in new tab");

    gBrowser.removeTab(newTab);
    gBrowser.removeTab(gBrowser.tabs[1]); // example.org tab
    finish();
  }
  newTab.linkedBrowser.loadURI("http://example.com");
}
