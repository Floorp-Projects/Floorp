/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported run_test */

/**
 * Test that things behave reasonably when a reasonable Hawk-Session-Token
 * header is returned with the registration response.
 */
add_test(function test_registration_returns_hawk_session_token() {
  var fakeSessionToken = "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1750";
  Services.prefs.clearUserPref("loop.hawk-session-token");

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Hawk-Session-Token", fakeSessionToken, false);
    response.processAsync();
    response.finish();
  });

  MozLoopService.promiseRegisteredWithServers().then(() => {
    var hawkSessionPref;
    try {
      hawkSessionPref = Services.prefs.getCharPref("loop.hawk-session-token");
    } catch (ex) {
      // Do nothing
    }
    Assert.equal(hawkSessionPref, fakeSessionToken, "Should store" +
      " Hawk-Session-Token header contents in loop.hawk-session-token pref");

    run_next_test();
  }, () => {
    do_throw("shouldn't error on a successful request");
  });
});

function run_test() {
  setupFakeLoopServer();

  mockPushHandler.registrationPushURL = kEndPointUrl;

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.hawk-session-token");
  });

  run_next_test();
}
