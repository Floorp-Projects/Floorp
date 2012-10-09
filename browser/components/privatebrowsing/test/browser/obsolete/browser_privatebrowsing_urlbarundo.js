/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the undo history of the URL bar is cleared when
// leaving the private browsing mode.

function test() {
  // initialization
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  // fill in the URL bar with something
  gURLBar.value = "some test value";

  ok(gURLBar.editor.transactionManager.numberOfUndoItems > 0,
     "The undo history for the URL bar should not be empty");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  is(gURLBar.editor.transactionManager.numberOfUndoItems, 0,
     "The undo history of the URL bar should be cleared after leaving the private browsing mode");

  // cleanup
  Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
}

