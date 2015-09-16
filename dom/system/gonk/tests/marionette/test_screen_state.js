/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

var Services = SpecialPowers.Services;

function testScreenState(on, expected, msg) {
  // send event to RadioInterface
  Services.obs.notifyObservers(null, 'screen-state-changed', on);
  // maybe rild/qemu needs some time to process the event
  window.setTimeout(function() {
    runEmulatorCmd('gsm report creg', function(result) {
      is(result.pop(), 'OK', '\'gsm report creg\' successful');
      ok(result.indexOf(expected) !== -1, msg);
      runNextTest();
    })}, 1000);
}

function testScreenStateDisabled() {
  testScreenState('off', '+CREG: 1', 'screen is disabled');
}

function testScreenStateEnabled() {
  testScreenState('on', '+CREG: 2', 'screen is enabled');
}

var tests = [
  testScreenStateDisabled,
  testScreenStateEnabled
];

function runNextTest() {
  let test = tests.shift();
  if (!test) {
    cleanUp();
    return;
  }

  test();
}

function cleanUp() {
  finish();
}

runNextTest();
