/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let tabOne = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tabTwo = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/");
  gBrowser.selectedTab = tabTwo;

  let browser = gBrowser.getBrowserForTab(tabTwo);
  BrowserTestUtils.browserLoaded(browser).then(() => {
    gBrowser.showOnlyTheseTabs([tabTwo]);

    is(gBrowser.visibleTabs.length, 1, "Only one tab is visible");

    let uris = PlacesCommandHook.uniqueCurrentPages;
    is(uris.length, 1, "Only one uri is returned");

    is(uris[0].uri.spec, tabTwo.linkedBrowser.currentURI.spec, "It's the correct URI");

    gBrowser.removeTab(tabOne);
    gBrowser.removeTab(tabTwo);
    for (let tab of gBrowser.tabs) {
      gBrowser.showTab(tab);
    }

    finish();
  });
}
