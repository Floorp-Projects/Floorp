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
  log("Seek up");
  var request = FMRadio.seekUp();
  ok(request);

  var seekUpIsCancelled = false;
  request.onerror = function() {
    seekUpIsCancelled = true;
  };

  var seekUpCompleted = false;
  request.onsuccess = function() {
    ok(!seekUpIsCancelled);
    seekUpCompleted = true;
  };

  log("Seek up");
  var cancelSeekReq = FMRadio.cancelSeek();
  ok(cancelSeekReq);

  // There are two possibilities which depends on the system
  // process scheduling (bug 911063 comment 0):
  //   - seekup action is canceled
  //   - seekup's onsuccess fires before cancelSeek's onerror

  cancelSeekReq.onsuccess = function() {
    ok(seekUpIsCancelled, "Seekup request failed.");
    cleanUp();
  };

  cancelSeekReq.onerror = function() {
    ok(seekUpCompleted);
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

