/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] },
      { entries: [{ url: "about:rights", triggeringPrincipal_base64 }] }
    ]
  }]
};

function test() {
  /** Test for Bug 615394 - Session Restore should notify when it is beginning and ending a restore **/
  waitForExplicitFinish();

  waitForBrowserState(testState, test_undoCloseTab);
}

function test_undoCloseTab() {
  let tab = gBrowser.tabs[1],
      busyEventCount = 0,
      readyEventCount = 0,
      reopenedTab;

  ss.setTabValue(tab, "foo", "bar");

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    reopenedTab = gBrowser.tabs[1];
    readyEventCount++;
    is(ss.getTabValue(reopenedTab, "foo"), "bar");
    ss.setTabValue(reopenedTab, "baz", "qux");
  }

  function onSSTabRestored(aEvent) {
    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(ss.getTabValue(reopenedTab, "baz"), "qux");
    is(reopenedTab.linkedBrowser.currentURI.spec, "about:rights");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);

    gBrowser.removeTab(gBrowser.tabs[1]);
    finish();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, { once: true });

  gBrowser.removeTab(tab);
  reopenedTab = ss.undoCloseTab(window, 0);
}

