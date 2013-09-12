/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 491168 **/

  waitForExplicitFinish();

  const REFERRER1 = "http://example.org/?" + Date.now();
  const REFERRER2 = "http://example.org/?" + Math.random();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    let tabState = JSON.parse(ss.getTabState(tab));
    is(tabState.entries[0].referrer,  REFERRER1,
       "Referrer retrieved via getTabState matches referrer set via loadURI.");

    tabState.entries[0].referrer = REFERRER2;
    ss.setTabState(tab, JSON.stringify(tabState));

    tab.addEventListener("SSTabRestored", function(e) {
      tab.removeEventListener("SSTabRestored", arguments.callee, true);
      is(window.content.document.referrer, REFERRER2, "document.referrer matches referrer set via setTabState.");

      gBrowser.removeTab(tab);
      // Call stopPropagation on the event so we won't fire the
      // tabbrowser's SSTabRestored listeners.
      e.stopPropagation();

      let newTab = ss.undoCloseTab(window, 0);
      newTab.addEventListener("SSTabRestored", function(e) {
        newTab.removeEventListener("SSTabRestored", arguments.callee, true);

        is(window.content.document.referrer, REFERRER2, "document.referrer is still correct after closing and reopening the tab.");
        gBrowser.removeTab(newTab);
        // Call stopPropagation on the event so we won't fire the
        // tabbrowser's SSTabRestored listeners.
        e.stopPropagation();

        finish();
      }, true);
    }, true);
  },true);

  let referrerURI = Services.io.newURI(REFERRER1, null, null);
  browser.loadURI("http://example.org", referrerURI, null);
}
