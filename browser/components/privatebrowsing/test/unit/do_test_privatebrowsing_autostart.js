/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks the browser.privatebrowsing.autostart preference.

function do_test() {
  // initialization
  var prefsService = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
  prefsService.setBoolPref("browser.privatebrowsing.autostart", true);
  do_check_true(prefsService.getBoolPref("browser.privatebrowsing.autostart"));

  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService).
           QueryInterface(Ci.nsIObserver);

  // private browsing not auto-started yet
  do_check_false(pb.autoStarted);

  // simulate startup to make the PB service read the prefs
  pb.observe(null, "profile-after-change", "");

  // the private mode should be entered automatically
  do_check_true(pb.privateBrowsingEnabled);

  // private browsing is auto-started
  do_check_true(pb.autoStarted);

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  // private browsing not auto-started
  do_check_false(pb.autoStarted);

  // enter private browsing mode again
  pb.privateBrowsingEnabled = true;

  // private browsing is auto-started
  do_check_true(pb.autoStarted);
}
