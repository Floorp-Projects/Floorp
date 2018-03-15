/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DUMMY_PAGE = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;

  BrowserTestUtils.loadURI(tab.linkedBrowser, DUMMY_PAGE);
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    gFindBar.onFindCommand();
    EventUtils.sendString("Dummy");
    gBrowser.removeTab(tab);

    try {
      gFindBar.close();
      ok(true, "findbar.close should not throw an exception");
    } catch (e) {
      ok(false, "findbar.close threw exception: " + e);
    }
    finish();
  });
}
