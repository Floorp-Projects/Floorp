/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const LOOP_HAWK_PREF = "loop.hawk-session-token";
const fakeSessionToken1 = "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1751";
const fakeSessionToken2 = "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1750";

add_test(function test_registration_invalid_token() {
  Services.prefs.setCharPref(LOOP_HAWK_PREF, fakeSessionToken1);
  var authorizationAttempts = 0;

  loopServer.registerPathHandler("/registration", (request, response) => {
    // Due to the way the time stamp checking code works in hawkclient, we expect a couple
    // of authorization requests before we reset the token.
    if (request.hasHeader("Authorization")) {
      authorizationAttempts++;
      response.setStatusLine(null, 401, "Unauthorized");
      response.write(JSON.stringify({
        code: 401,
        errno: 110,
        error: {
          error: "Unauthorized",
          message: "Unknown credentials",
          statusCode: 401
        }
      }));
    } else {
      // We didn't have an authorization header, so check the pref has been cleared.
      Assert.equal(Services.prefs.prefHasUserValue(LOOP_HAWK_PREF), false);
      response.setStatusLine(null, 200, "OK");
      response.setHeader("Hawk-Session-Token", fakeSessionToken2, false);
    }
    response.processAsync();
    response.finish();
  });

  MozLoopService.register(mockPushHandler).then(() => {
    // Due to the way the time stamp checking code works in hawkclient, we expect a couple
    // of authorization requests before we reset the token.
    Assert.equal(authorizationAttempts, 2);
    Assert.equal(Services.prefs.getCharPref(LOOP_HAWK_PREF), fakeSessionToken2);
    run_next_test();
  }, err => {
    do_throw("shouldn't be a failure result: " + err);
  });
});


function run_test()
{
  setupFakeLoopServer();

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.hawk-session-token");
  });

  run_next_test();
}
