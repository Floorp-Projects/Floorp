/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the URL bar is enabled after entering the private
// browsing mode, even if it's disabled before that (see bug 495495).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);

  // clear the history of closed windows (that other tests have created)
  // to avoid the issue in bug 596592
  // XXX remove this when bug 597071 is fixed
  while (ss.getClosedWindowCount())
    ss.forgetClosedWindow(0);

  // backup our state
  let stateBackup = ss.getWindowState(window);

  function pretendToBeAPopup(whatToPretend) {
    let state = whatToPretend ?
      '{"windows":[{"tabs":[{"entries":[],"attributes":{}}],"isPopup":true,"hidden":"toolbar"}]}' :
      '{"windows":[{"tabs":[{"entries":[],"attributes":{}}],"isPopup":false}]}';
    ss.setWindowState(window, state, true);
    if (whatToPretend) {
      ok(gURLBar.readOnly, "pretendToBeAPopup correctly made the URL bar read-only");
      is(gURLBar.getAttribute("enablehistory"), "false",
         "pretendToBeAPopup correctly disabled autocomplete on the URL bar");
    }
    else {
      ok(!gURLBar.readOnly, "pretendToBeAPopup correctly made the URL bar read-write");
      is(gURLBar.getAttribute("enablehistory"), "true",
         "pretendToBeAPopup correctly enabled autocomplete on the URL bar");
    }
  }

  // first, test the case of entering the PB mode when a popup window is active

  // pretend we're a popup window
  pretendToBeAPopup(true);

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  // make sure that the url bar status is correctly restored
  ok(!gURLBar.readOnly,
     "URL bar should not be read-only after entering the private browsing mode");
  is(gURLBar.getAttribute("enablehistory"), "true",
     "URL bar autocomplete should be enabled after entering the private browsing mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  // we're no longer a popup window
  pretendToBeAPopup(false);

  // then, test the case of leaving the PB mode when a popup window is active

  // start from within the private browsing mode
  pb.privateBrowsingEnabled = true;

  // pretend we're a popup window
  pretendToBeAPopup(true);

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  // make sure that the url bar status is correctly restored
  ok(!gURLBar.readOnly,
     "URL bar should not be read-only after leaving the private browsing mode");
  is(gURLBar.getAttribute("enablehistory"), "true",
     "URL bar autocomplete should be enabled after leaving the private browsing mode");

  // cleanup
  ss.setWindowState(window, stateBackup, true);
}
