/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks the browser.privatebrowsing.autostart preference.

function run_test() {
  PRIVATEBROWSING_CONTRACT_ID = "@mozilla.org/privatebrowsing-wrapper;1";
  load("do_test_privatebrowsing_autostart.js");
  do_test();
}
