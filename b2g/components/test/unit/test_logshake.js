/**
 * Test the log capturing capabilities of LogShake.jsm
 */

/* jshint moz: true */
/* global Components, LogCapture, LogShake, ok, add_test, run_next_test, dump */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */
'use strict';

const Cu = Components.utils;

Cu.import('resource://gre/modules/LogCapture.jsm');
Cu.import('resource://gre/modules/LogShake.jsm');

// Force logshake to handle a device motion event with given components
// Does not use SystemAppProxy because event needs special
// accelerationIncludingGravity property
function sendDeviceMotionEvent(x, y, z) {
  let event = {
    type: 'devicemotion',
    accelerationIncludingGravity: {
      x: x,
      y: y,
      z: z
    }
  };
  LogShake.handleEvent(event);
}

// Send a screen change event directly, does not use SystemAppProxy due to race
// conditions.
function sendScreenChangeEvent(screenEnabled) {
  let event = {
    type: 'screenchange',
    detail: {
      screenEnabled: screenEnabled
    }
  };
  LogShake.handleEvent(event);
}

function debug(msg) {
  var timestamp = Date.now();
  dump('LogShake: ' + timestamp + ': ' + msg);
}

add_test(function test_do_log_capture_after_shaking() {
  // Enable LogShake
  LogShake.init();

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    return null; // we don't want to provide invalid data to a parser
  };

  // Fire a devicemotion event that is of shake magnitude
  sendDeviceMotionEvent(9001, 9001, 9001);

  ok(readLocations.length > 0,
      'LogShake should attempt to read at least one log');

  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_nothing_when_resting() {
  // Enable LogShake
  LogShake.init();

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    return null; // we don't want to provide invalid data to a parser
  };

  // Fire a devicemotion event that is relatively tiny
  sendDeviceMotionEvent(0, 9.8, 9.8);

  ok(readLocations.length === 0,
      'LogShake should not read any logs');

  debug('test_do_nothing_when_resting: stop');
  LogShake.uninit();
  run_next_test();
});

add_test(function test_do_nothing_when_disabled() {
  debug('test_do_nothing_when_disabled: start');
  // Disable LogShake
  LogShake.uninit();

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    return null; // we don't want to provide invalid data to a parser
  };

  // Fire a devicemotion event that would normally be a shake
  sendDeviceMotionEvent(0, 9001, 9001);

  ok(readLocations.length === 0,
      'LogShake should not read any logs');

  run_next_test();
});

add_test(function test_do_nothing_when_screen_off() {
  // Enable LogShake
  LogShake.init();


  // Send an event as if the screen has been turned off
  sendScreenChangeEvent(false);

  let readLocations = [];
  LogCapture.readLogFile = function(loc) {
    readLocations.push(loc);
    return null; // we don't want to provide invalid data to a parser
  };

  // Fire a devicemotion event that would normally be a shake
  sendDeviceMotionEvent(0, 9001, 9001);

  ok(readLocations.length === 0,
      'LogShake should not read any logs');

  // Restore the screen
  sendScreenChangeEvent(true);

  LogShake.uninit();
  run_next_test();
});

function run_test() {
  debug('Starting');
  run_next_test();
}
