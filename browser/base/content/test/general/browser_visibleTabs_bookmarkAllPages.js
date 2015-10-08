/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let tabOne = gBrowser.addTab("about:blank");
  let tabTwo = gBrowser.addTab("http://mochi.test:8888/");
  gBrowser.selectedTab = tabTwo;

  let browser = gBrowser.getBrowserForTab(tabTwo);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);

    gBrowser.showOnlyTheseTabs([tabTwo]);

    is(gBrowser.visibleTabs.length, 1, "Only one tab is visible");

    let uris = PlacesCommandHook.uniqueCurrentPages;
    is(uris.length, 1, "Only one uri is returned");

    is(uris[0].spec, tabTwo.linkedBrowser.currentURI.spec, "It's the correct URI");

    gBrowser.removeTab(tabOne);
    gBrowser.removeTab(tabTwo);
    Array.forEach(gBrowser.tabs, function(tab) {
      gBrowser.showTab(tab);
    });

    finish();
  }
  browser.addEventListener("load", onLoad, true);
}
