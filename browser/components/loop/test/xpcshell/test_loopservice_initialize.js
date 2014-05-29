/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");

var server;

const kServerPushUrl = "http://localhost:3456";

/**
 * Tests that registration doesn't happen when the expiry time is
 * not set.
 */
add_test(function test_initialize_no_expiry() {
  MozLoopService.initialize(function(err) {
    Assert.equal(err, false,
      "should not register when no expiry time is set");

    run_next_test();
  });
});

/**
 * Tests that registration doesn't happen when the expiry time is
 * in the past.
 */
add_test(function test_initialize_expiry_past() {
  // Set time to be 2 seconds in the past.
  var nowSeconds = Date.now() / 1000;
  Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", nowSeconds - 2);

  MozLoopService.initialize(function(err) {
    Assert.equal(err, false,
      "should not register when expiry time is in past");

    run_next_test();
  });
});

/**
 * Tests that registration happens when the expiry time is in
 * the future.
 */
add_test(function test_initialize_and_register() {
  // Set time to be 1 minute in the future
  var nowSeconds = Date.now() / 1000;
  Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", nowSeconds + 60);

  MozLoopService.initialize(function(err) {
    Assert.equal(err, null,
      "should not register when expiry time is in past");

    run_next_test();
  });
});

function run_test()
{
  server = new HttpServer();
  server.start(-1);
  server.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  // Registrations and pref settings.
  gMockWebSocketChannelFactory.register();
  Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);
  Services.prefs.setCharPref("loop.server", "http://localhost:" + server.identity.primaryPort);

  // Set the initial registration delay to a short value for fast run tests.
  Services.prefs.setIntPref("loop.initialDelay", 10);

  do_register_cleanup(function() {
    gMockWebSocketChannelFactory.unregister();
    server.stop(function() {});
  });

  run_next_test();
}
