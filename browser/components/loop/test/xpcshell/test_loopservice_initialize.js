/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");

var server;

const kServerPushUrl = "http://localhost:3456";

var startTimerCalled = false;

/**
 * Tests that registration doesn't happen when the expiry time is
 * not set.
 */
add_task(function test_initialize_no_expiry() {
  startTimerCalled = false;

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, false,
    "should not register when no expiry time is set");
});

/**
 * Tests that registration doesn't happen when the expiry time is
 * in the past.
 */
add_task(function test_initialize_expiry_past() {
  // Set time to be 2 seconds in the past.
  var nowSeconds = Date.now() / 1000;
  Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", nowSeconds - 2);
  startTimerCalled = false;

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, false,
    "should not register when expiry time is in past");
});

/**
 * Tests that registration happens when the expiry time is in
 * the future.
 */
add_task(function test_initialize_starts_timer() {
  // Set time to be 1 minute in the future
  var nowSeconds = Date.now() / 1000;
  Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", nowSeconds + 60);
  startTimerCalled = false;

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, true,
    "should start the timer when expiry time is in the future");
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

  // Override MozLoopService's initializeTimer, so that we can verify the timeout is called
  // correctly.
  MozLoopService._startInitializeTimer = function() {
    startTimerCalled = true;
  };

  do_register_cleanup(function() {
    gMockWebSocketChannelFactory.unregister();
    server.stop(function() {});
  });

  run_next_test();
}
