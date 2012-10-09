/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the Clear Recent History menu item and command 
// is disabled inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  let crhCommand = document.getElementById("Tools:Sanitize");

  // make sure the command is not disabled to begin with
  ok(!crhCommand.hasAttribute("disabled"),
    "Clear Recent History command should not be disabled outside of the private browsing mode");

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  ok(crhCommand.hasAttribute("disabled"),
    "Clear Recent History command should be disabled inside of the private browsing mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  ok(!crhCommand.hasAttribute("disabled"),
    "Clear Recent History command should not be disabled after leaving the private browsing mode");
}
