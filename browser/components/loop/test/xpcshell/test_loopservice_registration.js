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

  MozLoopService.register(function(err) {
    Assert.equal(err, "offline", "Expected error of 'offline' to be returned");

    Services.io.offline = false;
    run_next_test();
  });
});

/**
 * Test that the websocket can be fully registered, and that a Loop server
 * failure is reported.
 */
add_test(function test_register_websocket_success_loop_server_fail() {
  MozLoopService.register(function(err) {
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
 * Tests that we get a success response when both websocket and Loop server
 * registration are complete.
 */
add_test(function test_register_success() {
  server.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  MozLoopService.register(function(err) {
    Assert.equal(err, null, "Expected no errors in websocket registration");

    let instances = gMockWebSocketChannelFactory.createdInstances;
    Assert.equal(instances.length, 1,
                 "Should create only one instance of websocket");

    run_next_test();
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
    server.stop(function() {});
  });

  run_next_test();

}
