/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("fmradio", true, document);

var FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio);
  is(FMRadio.enabled, false);
  setUp();
}

function setUp() {
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  FMRadio.enable(frequency);
  FMRadio.onenabled = seek;
}

function seek() {
  var request = FMRadio.seekUp();
  ok(request);

  // There are two possibilities which depends on the system
  // process scheduling (bug 911063 comment 0):
  //   - the second seek fails
  //   - both seeks are executed

  request.onerror = function() {
    ok(!firstSeekCompletes);
    cleanUp();
  };

  var firstSeekCompletes = false;
  request.onsuccess = function() {
    firstSeekCompletes = true;
  };

  var seekAgainReq = FMRadio.seekUp();
  ok(seekAgainReq);

  seekAgainReq.onerror = function() {
    log("Cancel the first seek to finish the test");
    let cancelReq = FMRadio.cancelSeek();
    ok(cancelReq);

    // It's possible that the first seek completes when the
    // cancel request is handled.
    cancelReq.onerror = function() {
      cleanUp();
    };
  };

  seekAgainReq.onsuccess = function() {
    ok(firstSeekCompletes);
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

