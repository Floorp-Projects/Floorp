/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the find bar is cleared when leaving the
// private browsing mode.

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // fill in the find bar with something
  const kTestSearchString = "privatebrowsing";
  let findBox = gFindBar.getElement("findbar-textbox");
  gFindBar.startFind(gFindBar.FIND_NORMAL);

  // sanity checks
  is(findBox.editor.transactionManager.numberOfUndoItems, 0,
    "No items in the undo list of the findbar control");
  is(findBox.value, "",
    "findbar text is empty");

  findBox.value = kTestSearchString;

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  is(findBox.value, kTestSearchString,
    "entering the private browsing mode should not clear the findbar");
  ok(findBox.editor.transactionManager.numberOfUndoItems > 0,
    "entering the private browsing mode should not reset the undo list of the findbar control");

  // Change the find bar value inside the private browsing mode
  findBox.value = "something else";

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  is(findBox.value, kTestSearchString,
    "leaving the private browsing mode should restore the findbar contents");
  is(findBox.editor.transactionManager.numberOfUndoItems, 1,
    "leaving the private browsing mode should only leave 1 item in the undo list of the findbar control");

  // cleanup
  gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
  gFindBar.close();
}
