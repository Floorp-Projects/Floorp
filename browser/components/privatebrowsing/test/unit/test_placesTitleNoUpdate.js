/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to make sure that the visited page titles do not get updated inside the
// private browsing mode.

function run_test() {
  PRIVATEBROWSING_CONTRACT_ID = "@mozilla.org/privatebrowsing;1";
  load("do_test_placesTitleNoUpdate.js");
  do_test();
}
