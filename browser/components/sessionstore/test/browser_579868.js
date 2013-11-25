/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let tab1 = gBrowser.addTab("about:rights");
  let tab2 = gBrowser.addTab("about:mozilla");
  whenBrowserLoaded(tab1.linkedBrowser, mainPart);
  waitForExplicitFinish();

  function mainPart() {
    // Tell the session storer that the tab is pinned
    let newTabState = '{"entries":[{"url":"about:rights"}],"pinned":true,"userTypedValue":"Hello World!"}';
    ss.setTabState(tab1, newTabState);

    // Undo pinning
    gBrowser.unpinTab(tab1);

    // Close and restore tab
    gBrowser.removeTab(tab1);
    let savedState = JSON.parse(ss.getClosedTabData(window))[0].state;
    isnot(savedState.pinned, true, "Pinned should not be true");
    tab1 = ss.undoCloseTab(window, 0);

    isnot(tab1.pinned, true, "Should not be pinned");
    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    finish();
  }
}
