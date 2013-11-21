/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let icc;
let connection;
let outgoing;

function changeSetting(key, value, callback) {
  let obj = {};
  obj[key] = value;

  let setReq = navigator.mozSettings.createLock().set(obj);
  setReq.addEventListener("success", function onSetSuccess() {
    ok(true, "set '" + key + "' to " + obj[key]);
    setReq.removeEventListener("success", onSetSuccess);
    callback();
  });
  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set '" + key + "'");
    cleanUp();
  });
}

function setRadioEnabled(enabled, callback) {
  changeSetting("ril.radio.disabled", !enabled, function() {
    // Wait for iccdetected event after turning on radio.
    // Wait for iccundetected event after turning off radio.
    let event = (enabled) ? "iccdetected" : "iccundetected";
    icc.addEventListener(event, function handler(evt) {
      log(event + ": " + evt.iccId);
      icc.removeEventListener(event, handler);
      callback();
    });
  });
}

function dial(number) {
  // Verify initial state before dial.
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);

  log("Make an outgoing call.");
  outgoing = telephony.dial(number);

  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  is(telephony.active, outgoing);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoing);

  outgoing.onerror = function onerror(event) {
    log("Received 'error' event.");
    is(event.call, outgoing);
    ok(event.call.error);
    is(event.call.error.name, "RadioNotAvailable");

    emulator.run("gsm list", function(result) {
      log("Initial call list: " + result);
      is(result[0], "OK");

      setRadioEnabled(true, cleanUp);
    });
  };
}

function cleanUp() {
  finish();
}

let permissions = [
  "mobileconnection",
  "settings-write"
];

startTestWithPermissions(permissions, function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  icc = navigator.mozIccManager;
  ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

  setRadioEnabled(false, function() {
    dial("0912345678");
  });
});
