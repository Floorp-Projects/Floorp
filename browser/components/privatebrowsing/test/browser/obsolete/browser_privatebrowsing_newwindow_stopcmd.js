/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the stop private browsing command is enabled in
// new windows opened from the private browsing mode (bug 529667).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();

  pb.privateBrowsingEnabled = true;

  let win = OpenBrowserWindow();
  win.addEventListener("load", function() {
    win.removeEventListener("load", arguments.callee, false);
    executeSoon(function() {
      let cmd = win.document.getElementById("Tools:PrivateBrowsing");
      ok(!cmd.hasAttribute("disabled"),
         "The Private Browsing command in a new window should be enabled");

      win.close();
      pb.privateBrowsingEnabled = false;
      finish();
    });
  }, false);
}
