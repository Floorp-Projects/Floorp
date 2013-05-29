/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

is(icc.cardState, "ready");

function setAirplaneModeEnabled(enabled) {
  let settings = window.navigator.mozSettings;
  let setLock = settings.createLock();
  let obj = {
    "ril.radio.disabled": enabled
  };
  let setReq = setLock.set(obj);

  log("set airplane mode to " + enabled);

  setReq.addEventListener("success", function onSetSuccess() {
    log("set 'ril.radio.disabled' to " + enabled);
  });

  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set 'ril.radio.disabled' to " + enabled);
  });
}

function waitCardStateChangedEvent(expectedCardState, callback) {
  icc.addEventListener("cardstatechange", function oncardstatechange() {
    log("card state changes to " + icc.cardState);
    if (icc.cardState === expectedCardState) {
      log("got expected card state: " + icc.cardState);
      icc.removeEventListener("cardstatechange", oncardstatechange);
      callback();
    }
  });
}

// Test cardstatechange event by switching airplane mode
function testCardStateChange(airplaneMode, expectedCardState, callback) {
  setAirplaneModeEnabled(airplaneMode);
  waitCardStateChangedEvent(expectedCardState, callback);
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.removePermission("settings-write", document);

  finish();
}

// Enable Airplane mode, expect got cardstatechange to null
testCardStateChange(true, null,
  // Disable Airplane mode, expect got cardstatechange to 'ready'
  testCardStateChange.bind(this, false, "ready", cleanUp)
);