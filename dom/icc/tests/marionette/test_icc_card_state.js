/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);
SpecialPowers.addPermission("settings-write", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let icc;
ifr.onload = function() {
  icc = ifr.contentWindow.navigator.mozIccManager;

  ok(icc instanceof ifr.contentWindow.MozIccManager,
     "icc is instanceof " + icc.constructor);

  is(icc.cardState, "ready");

  // Enable Airplane mode, expect got cardstatechange to null
  testCardStateChange(true, null,
    // Disable Airplane mode, expect got cardstatechange to 'ready'
    testCardStateChange.bind(window, false, "ready", cleanUp)
  );
};
document.body.appendChild(ifr);

function setAirplaneModeEnabled(enabled) {
  let settings = ifr.contentWindow.navigator.mozSettings;
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
