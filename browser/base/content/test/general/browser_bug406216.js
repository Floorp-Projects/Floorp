/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * "TabClose" event is possibly used for closing related tabs of the current.
 * "removeTab" method should work correctly even if the number of tabs are
 * changed while "TabClose" event.
 */

var count = 0;
const URIS = ["about:config",
              "about:plugins",
              "about:buildconfig",
              "data:text/html,<title>OK</title>"];

function test() {
  waitForExplicitFinish();
  URIS.forEach(addTab);
}

function addTab(aURI, aIndex) {
  var tab = gBrowser.addTab(aURI);
  if (aIndex == 0)
    gBrowser.removeTab(gBrowser.tabs[0]);

  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    if (++count == URIS.length)
      executeSoon(doTabsTest);
  }, true);
}

function doTabsTest() {
  is(gBrowser.tabs.length, URIS.length, "Correctly opened all expected tabs");

  // sample of "close related tabs" feature
  gBrowser.tabContainer.addEventListener("TabClose", function (event) {
    event.currentTarget.removeEventListener("TabClose", arguments.callee, true);
    var closedTab = event.originalTarget;
    var scheme = closedTab.linkedBrowser.currentURI.scheme;
    Array.slice(gBrowser.tabs).forEach(function (aTab) {
      if (aTab != closedTab && aTab.linkedBrowser.currentURI.scheme == scheme)
        gBrowser.removeTab(aTab);
    });
  }, true);

  gBrowser.removeTab(gBrowser.tabs[0]);
  is(gBrowser.tabs.length, 1, "Related tabs are not closed unexpectedly");

  gBrowser.addTab("about:blank");
  gBrowser.removeTab(gBrowser.tabs[0]);
  finish();
}
