/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported run_test */

var startTimerCalled = false;

/**
 * Tests that registration doesn't happen when the expiry time is
 * not set.
 */
add_task(function* test_initialize_no_expiry() {
  startTimerCalled = false;

  let initializedPromise = yield MozLoopService.initialize();
  Assert.equal(initializedPromise, "registration not needed",
               "Promise should be fulfilled");
  Assert.equal(startTimerCalled, false,
    "should not register when no expiry time is set");
});

/**
 * Tests that registration doesn't happen when there has been no
 * room created.
 */
add_task(function test_initialize_no_guest_rooms() {
  Services.prefs.setBoolPref("loop.createdRoom", false);
  startTimerCalled = false;

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, false,
    "should not register when no guest rooms have been created");
});

/**
 * Tests that registration happens when the expiry time is in
 * the future.
 */
add_task(function test_initialize_with_guest_rooms() {
  Services.prefs.setBoolPref("loop.createdRoom", true);
  startTimerCalled = false;
  MozLoopService.resetServiceInitialized();

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, true,
    "should start the timer when guest rooms have been created");

  startTimerCalled = false;

  MozLoopService.initialize();

  Assert.equal(startTimerCalled, false,
    "should not have initialized a second time");
});

function run_test() {
  setupFakeLoopServer();

  // Override MozLoopService's initializeTimer, so that we can verify the timeout is called
  // correctly.
  MozLoopService.initializeTimerFunc = function() {
    startTimerCalled = true;
  };

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.createdRoom");
  });

  run_next_test();
}
