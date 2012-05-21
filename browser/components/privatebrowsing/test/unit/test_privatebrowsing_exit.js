/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the private browsing mode is left at application
// shutdown.

function run_test() {
  PRIVATEBROWSING_CONTRACT_ID = "@mozilla.org/privatebrowsing;1";
  load("do_test_privatebrowsing_exit.js");
  do_test();
}
