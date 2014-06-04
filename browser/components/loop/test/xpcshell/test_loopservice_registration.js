/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");

var server;

const kServerPushUrl = "http://localhost:3456";

/**
 * Tests reported failures when we're in offline mode.
 */
add_test(function test_register_offline() {
  // It should callback with failure if in offline mode
  Services.io.offline = true;

  MozLoopService.register().then(() => {
    do_throw("should not succeed when offline");
  }, err => {
    Assert.equal(err, "offline", "should reject with 'offline' when offline");
    Services.io.offline = false;
    run_next_test();
  });
});

/**
 * Test that the websocket can be fully registered, and that a Loop server
 * failure is reported.
 */
add_test(function test_register_websocket_success_loop_server_fail() {
  MozLoopService.register().then(() => {
    do_throw("should not succeed when loop server registration fails");
  }, err => {
    // 404 is an expected failure indicated by the lack of route being set
    // up on the Loop server mock. This is added in the next test.
    Assert.equal(err, 404, "Expected no errors in websocket registration");

    let instances = gMockWebSocketChannelFactory.createdInstances;
    Assert.equal(instances.length, 1,
                 "Should create only one instance of websocket");
    Assert.equal(instances[0].uri.prePath, kServerPushUrl,
                 "Should have the url from preferences");
    Assert.equal(instances[0].origin, kServerPushUrl,
                 "Should have the origin url from preferences");
    Assert.equal(instances[0].protocol, "push-notification",
                 "Should have the protocol set to push-notifications");

    gMockWebSocketChannelFactory.resetInstances();

    run_next_test();
  });
});

/**
 * Test that things behave reasonably when a reasonable Hawk-Session-Token
 * header is returned with the registration response.
 */
add_test(function test_registration_returns_hawk_session_token() {

  var fakeSessionToken = "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1750";
  Services.prefs.clearUserPref("loop.hawk-session-token");

  server.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Hawk-Session-Token", fakeSessionToken, false);
    response.processAsync();
    response.finish();
  });

  MozLoopService.register().then(() => {
    var hawkSessionPref;
    try {
      hawkSessionPref = Services.prefs.getCharPref("loop.hawk-session-token");
    } catch (ex) {
    }
    Assert.equal(hawkSessionPref, fakeSessionToken, "Should store" +
      " Hawk-Session-Token header contents in loop.hawk-session-token pref");

    run_next_test();
  }, err => {
    do_throw("shouldn't error on a succesful request");
  });
});

// XXX should send existing token pref if already set

// XXX should call back the error string "no-hawk-token" and when
// Hawk-Session-Token is unset if we're waiting for one

/**
 * Tests that we get a success response when both websocket and Loop server
 * registration are complete.
 */
add_test(function test_register_success() {
  server.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });
  MozLoopService.register().then(() => {
    let instances = gMockWebSocketChannelFactory.createdInstances;
    Assert.equal(instances.length, 1,
                 "Should create only one instance of websocket");

    run_next_test();
  }, err => {
    do_throw("shouldn't error on a succesful request");
  });
});

function run_test()
{
  server = new HttpServer();
  server.start(-1);

  // Registrations and pref settings.
  gMockWebSocketChannelFactory.register();
  Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);

  Services.prefs.setCharPref("loop.server", "http://localhost:" + server.identity.primaryPort);

  do_register_cleanup(function() {
    gMockWebSocketChannelFactory.unregister();
    Services.prefs.clearUserPref("loop.hawk-session-token");
    server.stop(function() {});
  });

  run_next_test();

}
