/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 367052 **/

  waitForExplicitFinish();

  // make sure that the next closed tab will increase getClosedTabCount
  let max_tabs_undo = gPrefService.getIntPref("browser.sessionstore.max_tabs_undo");
  gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", max_tabs_undo + 1);
  let closedTabCount = ss.getClosedTabCount(window);

  // restore a blank tab
  let tab = gBrowser.addTab("about:");
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);

    let history = tab.linkedBrowser.webNavigation.sessionHistory;
    ok(history.count >= 1, "the new tab does have at least one history entry");

    ss.setTabState(tab, JSON.stringify({ entries: [] }));
    tab.linkedBrowser.addEventListener("load", function(aEvent) {
      this.removeEventListener("load", arguments.callee, true);
      ok(history.count == 0, "the tab was restored without any history whatsoever");

      gBrowser.removeTab(tab);
      ok(ss.getClosedTabCount(window) == closedTabCount,
         "The closed blank tab wasn't added to Recently Closed Tabs");

      // clean up
      if (gPrefService.prefHasUserValue("browser.sessionstore.max_tabs_undo"))
        gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
      finish();
    }, true);
  }, true);
}
