/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("fmradio", true, document);

let FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio);
  is(FMRadio.enabled, false);
  setUp();
}

function setUp() {
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  FMRadio.enable(frequency);
  FMRadio.onenabled = setFrequency;
}

function setFrequency() {
  log("Set Frequency");
  let frequency = FMRadio.frequency + FMRadio.channelWidth;
  var request = FMRadio.setFrequency(frequency);
  ok(request);

  request.onsuccess = setOutOfRangeFrequency;
  request.onerror = function() {
    ok(false, "setFrequency request should not fail.");
  };
}

function setOutOfRangeFrequency() {
  log("Set Frequency that out of the range");
  var request = FMRadio.setFrequency(FMRadio.frequencyUpperBound + 1);
  ok(request);

  request.onsuccess = function() {
    ok(false, "The request of setting an out-of-range frequency should fail.");
  };
  request.onerror = setFrequencyWhenSeeking;
}

function setFrequencyWhenSeeking() {
  log("Set frequency when seeking");
  var request = FMRadio.seekUp();
  ok(request);

  // There are two possibilities which depends on the system
  // process scheduling (bug 911063 comment 0):
  //   - seek fails
  //   - seek's onsuccess fires before setFrequency's onsucess

  var failedToSeek = false;
  request.onerror = function() {
    failedToSeek = true;
  };

  var seekCompletes = false;
  request.onsuccess = function() {
    ok(!failedToSeek);
    seekCompletes = true;
  };

  var frequency = FMRadio.frequencyUpperBound - FMRadio.channelWidth;
  var setFreqRequest = FMRadio.setFrequency(frequency);
  ok(setFreqRequest);

  setFreqRequest.onsuccess = function() {
    ok(failedToSeek || seekCompletes);
    cleanUp();
  };
}

function cleanUp() {
  FMRadio.disable();
  FMRadio.ondisabled = function() {
    FMRadio.ondisabled = null;
    ok(!FMRadio.enabled);
    finish();
  };
}

verifyInitialState();

