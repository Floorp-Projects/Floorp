/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 447951 **/

  waitForExplicitFinish();
  const baseURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_447951_sample.html#";

  let tab = gBrowser.addTab();
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    let tabState = { entries: [] };
    let max_entries = gPrefService.getIntPref("browser.sessionhistory.max_entries");
    for (let i = 0; i < max_entries; i++)
      tabState.entries.push({ url: baseURL + i });

    ss.setTabState(tab, JSON.stringify(tabState));
    tab.addEventListener("SSTabRestored", function(aEvent) {
      tab.removeEventListener("SSTabRestored", arguments.callee, false);
      tabState = JSON.parse(ss.getTabState(tab));
      is(tabState.entries.length, max_entries, "session history filled to the limit");
      is(tabState.entries[0].url, baseURL + 0, "... but not more");

      // visit yet another anchor (appending it to session history)
      let doc = tab.linkedBrowser.contentDocument;
      let event = doc.createEvent("MouseEvents");
      event.initMouseEvent("click", true, true, doc.defaultView, 1,
                           0, 0, 0, 0, false, false, false, false, 0, null);
      doc.querySelector("a").dispatchEvent(event);

      executeSoon(function() {
        tabState = JSON.parse(ss.getTabState(tab));
        is(tab.linkedBrowser.currentURI.spec, baseURL + "end",
           "the new anchor was loaded");
        is(tabState.entries[tabState.entries.length - 1].url, baseURL + "end",
           "... and ignored");
        is(tabState.entries[0].url, baseURL + 1,
           "... and the first item was removed");

        // clean up
        gBrowser.removeTab(tab);
        finish();
      });
    }, false);
  }, true);
}
