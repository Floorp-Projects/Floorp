/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const KEY = "ril.radio.disabled";

let gSettingsEnabled = SpecialPowers.getBoolPref("dom.mozSettings.enabled");
if (!gSettingsEnabled) {
  SpecialPowers.setBoolPref("dom.mozSettings.enabled", true);
}
SpecialPowers.addPermission("telephony", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let settings = window.navigator.mozSettings;
let telephony = window.navigator.mozTelephony;
let number = "112";
let outgoing;

function verifyInitialState() {
  log("Turning on airplane mode");

  let setLock = settings.createLock();
  let obj = {};
  obj[KEY] = false;
  let setReq = setLock.set(obj);
  setReq.addEventListener("success", function onSetSuccess() {
    ok(true, "set '" + KEY + "' to " + obj[KEY]);
  });
  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set '" + KEY + "'");
  });

  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);

  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    is(result[0], "OK");
    if (result[0] == "OK") {
      dial();
    } else {
      log("Call exists from a previous test, failing out.");
      cleanUp();
    }
  });
}

function dial() {
  log("Make an outgoing call.");

  outgoing = telephony.dial(number);
  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  is(outgoing, telephony.active);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoing);

  outgoing.onalerting = function onalerting(event) {
    log("Received 'onalerting' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "alerting");

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + "        : ringing");
      is(result[1], "OK");
      answer();
    });
  };
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoing.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "connected");

    is(outgoing, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + "        : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  runEmulatorCmd("gsm accept " + number);
};

function hangUp() {
  log("Hanging up the outgoing call.");

  // We get no "disconnecting" event when the remote party terminates the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  runEmulatorCmd("gsm cancel " + number);
}

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  SpecialPowers.removePermission("settings-write", document);
  SpecialPowers.clearUserPref("dom.mozSettings.enabled");
  finish();
}

verifyInitialState();
