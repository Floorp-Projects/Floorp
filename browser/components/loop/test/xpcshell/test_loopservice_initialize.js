/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var startTimerCalled = false;

/**
 * Tests that registration doesn't happen when the expiry time is
 * not set.
 */
add_task(function test_initialize_no_expiry() {
  startTimerCalled = false;

  let initializedPromise = yield MozLoopService.initialize();
  Assert.equal(initializedPromise, "registration not needed",
               "Promise should be fulfilled");
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
  setupFakeLoopServer();

  // Override MozLoopService's initializeTimer, so that we can verify the timeout is called
  // correctly.
  MozLoopService.initializeTimerFunc = function() {
    startTimerCalled = true;
  };

  run_next_test();
}
