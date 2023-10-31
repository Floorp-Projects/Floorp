/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * "TabClose" event is possibly used for closing related tabs of the current.
 * "removeTab" method should work correctly even if the number of tabs are
 * changed while "TabClose" event.
 */

var count = 0;
const URIS = [
  "about:config",
  "about:robots",
  "about:buildconfig",
  "data:text/html,<title>OK</title>",
];

function test() {
  waitForExplicitFinish();
  URIS.forEach(addTab);
}

function addTab(aURI, aIndex) {
  var tab = BrowserTestUtils.addTab(gBrowser, aURI);
  if (aIndex == 0) {
    gBrowser.removeTab(gBrowser.tabs[0], { skipPermitUnload: true });
  }

  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    if (++count == URIS.length) {
      executeSoon(doTabsTest);
    }
  });
}

function doTabsTest() {
  is(gBrowser.tabs.length, URIS.length, "Correctly opened all expected tabs");

  // sample of "close related tabs" feature
  gBrowser.tabContainer.addEventListener(
    "TabClose",
    function (event) {
      var closedTab = event.originalTarget;
      var scheme = closedTab.linkedBrowser.currentURI.scheme;
      Array.from(gBrowser.tabs).forEach(function (aTab) {
        if (
          aTab != closedTab &&
          aTab.linkedBrowser.currentURI.scheme == scheme
        ) {
          gBrowser.removeTab(aTab, { skipPermitUnload: true });
        }
      });
    },
    { capture: true, once: true }
  );

  gBrowser.removeTab(gBrowser.tabs[0], { skipPermitUnload: true });
  is(gBrowser.tabs.length, 1, "Related tabs are not closed unexpectedly");

  BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.removeTab(gBrowser.tabs[0], { skipPermitUnload: true });
  finish();
}
