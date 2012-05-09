/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DUMMY_PAGE = "http://example.org/browser/browser/base/content/test/dummy_page.html";

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  load(tab, DUMMY_PAGE, function() {
    gFindBar.onFindCommand();
    EventUtils.sendString("Dummy");
    gBrowser.removeTab(tab);

    try {
      gFindBar.close();
      ok(true, "findbar.close should not throw an exception");
    } catch(e) {
      ok(false, "findbar.close threw exception: " + e);
    }
    finish();
  });
}

function load(aTab, aUrl, aCallback) {
  aTab.linkedBrowser.addEventListener("load", function onload(aEvent) {
    aEvent.currentTarget.removeEventListener("load", onload, true);
    waitForFocus(aCallback, content);
  }, true);
  aTab.linkedBrowser.loadURI(aUrl);
}
