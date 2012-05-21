/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that privatebrowsingmode attribute of the window is correctly
// switched with private browsing mode changes.

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let docRoot = document.documentElement;

  ok(!docRoot.hasAttribute("privatebrowsingmode"),
    "privatebrowsingmode should not be present in normal mode");

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  is(docRoot.getAttribute("privatebrowsingmode"), "temporary",
    "privatebrowsingmode should be \"temporary\" inside the private browsing mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  ok(!docRoot.hasAttribute("privatebrowsingmode"),
    "privatebrowsingmode should not be present in normal mode");

  // cleanup
  gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
}
