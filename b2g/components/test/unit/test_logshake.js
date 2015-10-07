/**
 * Test the log capturing capabilities of LogShake.jsm
 */

/* jshint moz: true */
/* global Components, LogCapture, LogShake, ok, add_test, run_next_test, dump */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */
"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/LogCapture.jsm");
Cu.import("resource://gre/modules/LogShake.jsm");

const EVENTS_PER_SECOND = 6.25;
const GRAVITY = 9.8;

/**
 * Force logshake to handle a device motion event with given components.
 * Does not use SystemAppProxy because event needs special
 * accelerationIncludingGravity property.
 */
function sendDeviceMotionEvent(x, y, z) {
  let event = {
    type: "devicemotion",
    accelerationIncludingGravity: {
      x: x,
      y: y,
      z: z
    }
  };
  LogShake.handleEvent(event);
}

/**
 * Send a screen change event directly, does not use SystemAppProxy due to race
 * conditions.
 */
function sendScreenChangeEvent(screenEnabled) {
  let event = {
    type: "screenchange",
    detail: {
      screenEnabled: screenEnabled
    }
  };
  LogShake.handleEvent(event);
}

/**
 * Mock the readLogFile function of LogCapture.
 * Used to detect whether LogShake activates.
 * @return {Array<String>} Locations that LogShake tries to read
 */
function mockReadLogFile() {
  let readLocations = [];

  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    return null; // we don't want to provide invalid data to a parser
  };

  // Allow inspection of readLocations by caller
  return readLocations;
}

/**
 * Send a series of events that corresponds to a shake
 */
function sendSustainedShake() {
  // Fire a series of devicemotion events that are of shake magnitude
  for (let i = 0; i < 2 * EVENTS_PER_SECOND; i++) {
    sendDeviceMotionEvent(0, 2 * GRAVITY, 2 * GRAVITY);
  }

}

add_test(function test_do_log_capture_after_shaking() {
  // Enable LogShake
  LogShake.init();

  let readLocations = mockReadLogFile();

  sendSustainedShake();

  ok(readLocations.length > 0,
      "LogShake should attempt to read at least one log");

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_nothing_when_resting() {
  // Enable LogShake
  LogShake.init();

  let readLocations = mockReadLogFile();

  // Fire several devicemotion events that are relatively tiny
  for (let i = 0; i < 2 * EVENTS_PER_SECOND; i++) {
    sendDeviceMotionEvent(0, GRAVITY, GRAVITY);
  }

  ok(readLocations.length === 0,
      "LogShake should not read any logs");

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_nothing_when_disabled() {
  // Disable LogShake
  LogShake.uninit();

  let readLocations = mockReadLogFile();

  // Fire a series of events that would normally be a shake
  sendSustainedShake();

  ok(readLocations.length === 0,
      "LogShake should not read any logs");

  run_next_test();
});

add_test(function test_do_nothing_when_screen_off() {
  // Enable LogShake
  LogShake.init();

  // Send an event as if the screen has been turned off
  sendScreenChangeEvent(false);

  let readLocations = mockReadLogFile();

  // Fire a series of events that would normally be a shake
  sendSustainedShake();

  ok(readLocations.length === 0,
      "LogShake should not read any logs");

  // Restore the screen
  sendScreenChangeEvent(true);

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_log_capture_resilient_readLogFile() {
  // Enable LogShake
  LogShake.init();

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    throw new Error("Exception during readLogFile for: " + loc);
  };

  // Fire a series of events that would normally be a shake
  sendSustainedShake();

  ok(readLocations.length > 0,
      "LogShake should attempt to read at least one log");

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_log_capture_resilient_parseLog() {
  // Enable LogShake
  LogShake.init();

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    LogShake.LOGS_WITH_PARSERS[loc] = function() {
      throw new Error("Exception during LogParser for: " + loc);
    };
    return null;
  };

  // Fire a series of events that would normally be a shake
  sendSustainedShake();

  ok(readLocations.length > 0,
      "LogShake should attempt to read at least one log");

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_nothing_when_dropped() {
  // Enable LogShake
  LogShake.init();

  let readLocations = mockReadLogFile();

  // We want a series of spikes to be ignored by LogShake. This roughly
  // corresponds to the compare_stairs_sock graph on bug #1101994

  for (let i = 0; i < 10 * EVENTS_PER_SECOND; i++) {
    // Fire a devicemotion event that is at rest
    sendDeviceMotionEvent(0, 0, GRAVITY);
    // Fire a spike of motion
    sendDeviceMotionEvent(0, 2 * GRAVITY, 2 * GRAVITY);
  }

  ok(readLocations.length === 0,
      "LogShake should not read any logs");

  LogShake.uninit();
  run_next_test();
});

function run_test() {
  run_next_test();
}
