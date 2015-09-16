/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("fmradio", true, document);

var FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio);
  is(FMRadio.enabled, false);
  enableThenDisable();
}

function enableThenDisable() {
  log("Enable FM Radio and disable it immediately.");
  var frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  var request = FMRadio.enable(frequency);
  ok(request);

  var failedToEnable = false;
  request.onerror = function() {
    failedToEnable = true;
  };

  var enableCompleted = false;
  request.onsuccess = function() {
    ok(!failedToEnable);
    enableCompleted = true;
  };

  var disableReq = FMRadio.disable();
  ok(disableReq);

  disableReq.onsuccess = function() {
    // There are two possibilities which depends on the system
    // process scheduling (bug 911063 comment 0):
    //   - enable fails
    //   - enable's onsuccess fires before disable's onsucess
    ok(failedToEnable || enableCompleted);
    ok(!FMRadio.enabled);
    finish();
  };

  disableReq.onerror = function() {
    ok(false, "Disable request should not fail.");
  };
}

verifyInitialState();

