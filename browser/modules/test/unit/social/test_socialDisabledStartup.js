/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  // we are testing worker startup specifically
  do_test_pending();
  add_test(testStartupDisabled);
  add_test(testEnableAfterStartup);
  do_initialize_social(false, run_next_test);
}

function testStartupDisabled() {
  // wait on startup before continuing
  do_check_false(Social.enabled, "Social is disabled");
  do_check_eq(Social.providers.length, 0, "zero social providers available");
  run_next_test();
}

function testEnableAfterStartup() {
  do_add_providers(function () {
    do_check_true(Social.enabled, "Social is enabled");
    do_check_eq(Social.providers.length, 2, "two social providers available");
    do_check_true(Social.providers[0].enabled, "provider 0 is enabled");
    do_check_true(Social.providers[1].enabled, "provider 1 is enabled");
    do_test_finished();
    run_next_test();
  });
}
