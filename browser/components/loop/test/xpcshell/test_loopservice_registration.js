/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

/**
 * This file is to test general registration. Note that once successful
 * registration has taken place, we can no longer test the server side
 * parts as the service protects against this, hence, they need testing in
 * other test files.
 */

/**
 * Tests reported failures when we're in offline mode.
 */
add_test(function test_register_offline() {
  mockPushHandler.registrationResult = "offline";

  // It should callback with failure if in offline mode
  Services.io.offline = true;

  MozLoopService.register(mockPushHandler).then(() => {
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
  mockPushHandler.registrationResult = null;

  MozLoopService.register(mockPushHandler).then(() => {
    do_throw("should not succeed when loop server registration fails");
  }, err => {
    // 404 is an expected failure indicated by the lack of route being set
    // up on the Loop server mock. This is added in the next test.
    Assert.equal(err, 404, "Expected no errors in websocket registration");

    run_next_test();
  });
});

/**
 * Tests that we get a success response when both websocket and Loop server
 * registration are complete.
 */
add_test(function test_register_success() {
  mockPushHandler.registrationPushURL = kEndPointUrl;

  loopServer.registerPathHandler("/registration", (request, response) => {
    let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    let data = JSON.parse(body);
    Assert.equal(data.simplePushURL, kEndPointUrl,
                 "Should send correct push url");

    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });
  MozLoopService.register(mockPushHandler).then(() => {
    run_next_test();
  }, err => {
    do_throw("shouldn't error on a successful request");
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
