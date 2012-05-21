/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the search bar is cleared when leaving the
// private browsing mode.

function test() {
  // initialization
  waitForExplicitFinish();
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // fill in the search bar with something, twice to populate undo history
  const kTestSearchString = "privatebrowsing";
  let searchBar = BrowserSearch.searchBar;
  searchBar.value = kTestSearchString + "foo";
  searchBar.value = kTestSearchString;

  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  registerCleanupFunction(function () {
    searchBar.textbox.reset();

    gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
  });

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  is(searchBar.value, kTestSearchString,
    "entering the private browsing mode should not clear the search bar");
  ok(searchBar.textbox.editor.transactionManager.numberOfUndoItems > 0,
    "entering the private browsing mode should not reset the undo list of the searchbar control");

  // Change the search bar value inside the private browsing mode
  searchBar.value = "something else";

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  is(searchBar.value, kTestSearchString,
    "leaving the private browsing mode should restore the search bar contents");
  is(searchBar.textbox.editor.transactionManager.numberOfUndoItems, 1,
    "leaving the private browsing mode should only leave 1 item in the undo list of the searchbar control");

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  const TEST_URL =
    "data:text/html,<head><link rel=search type='application/opensearchdescription+xml' href='http://foo.bar' title=dummy></head>";
  gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
  gBrowser.selectedBrowser.addEventListener("load", function(e) {
    e.currentTarget.removeEventListener("load", arguments.callee, true);

    var browser = gBrowser.selectedBrowser;
    is(typeof browser.engines, "undefined",
       "An engine should not be discovered in private browsing mode");

    gBrowser.removeTab(gBrowser.selectedTab);
    pb.privateBrowsingEnabled = false;

    finish();
  }, true);
}
