/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported run_test */

add_test(function test_get_do_not_disturb() {
  Services.prefs.setBoolPref("loop.do_not_disturb", false);

  do_check_false(MozLoopService.doNotDisturb);

  Services.prefs.setBoolPref("loop.do_not_disturb", true);

  do_check_true(MozLoopService.doNotDisturb);

  run_next_test();
});

add_test(function test_set_do_not_disturb() {
  Services.prefs.setBoolPref("loop.do_not_disturb", false);

  MozLoopService.doNotDisturb = true;

  do_check_true(Services.prefs.getBoolPref("loop.do_not_disturb"));

  run_next_test();
});

function run_test() {
  setupFakeLoopServer();

  setupFakeFxAUserProfile();

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });
  loopServer.registerPathHandler("/calls", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify({ calls: [{ callId: 4444333221, websocketToken: "0deadbeef0" }] }));
    response.processAsync();
    response.finish();
  });

  do_register_cleanup(function() {
    // clear test pref
    Services.prefs.clearUserPref("loop.do_not_disturb");
  });

  run_next_test();
}
