/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

var battery = null;
var fromStatus = "charging";
var fromCharging = true;

function verifyInitialState() {
  window.navigator.getBattery().then(function (b) {
    battery = b;
    ok(battery, "battery");
    ok(battery.charging, "battery.charging");
    runEmulatorCmd("power display", function (result) {
      is(result.pop(), "OK", "power display successful");
      ok(result.indexOf("status: Charging") !== -1, "power status charging");
      setUp();
    });
  });
}

function unexpectedEvent(event) {
  ok(false, "Unexpected " + event.type + " event");
}

function setUp() {
  battery.onchargingchange = unexpectedEvent;
  battery.onlevelchange = unexpectedEvent;
  toDischarging();
}

function resetStatus(charging, nextFunction) {
  log("Resetting power status to " + fromStatus);
  if (charging !== fromCharging) {
    battery.onchargingchange = function () {
      battery.onchargingchange = unexpectedEvent;
      nextFunction();
    };
    runEmulatorCmd("power status " + fromStatus);
  }
  else {
    runEmulatorCmd("power status " + fromStatus, nextFunction);
  }
}

function changeStatus(toStatus, toCharging, nextFunction) {
  log("Changing power status to " + toStatus);
  if (fromCharging !== toCharging) {
    battery.onchargingchange = function (event) {
      battery.onchargingchange = unexpectedEvent;
      is(event.type, "chargingchange", "event type");
      is(battery.charging, toCharging, "battery.charging");
      resetStatus(toCharging, nextFunction);
    };
    runEmulatorCmd("power status " + toStatus);
  }
  else {
    runEmulatorCmd("power status " + toStatus, function () {
      is(battery.charging, toCharging, "battery.charging");
      resetStatus(toCharging, nextFunction);
    });
  }
}

function toDischarging() {
  changeStatus("discharging", false, toFull);
}

function toFull() {
  changeStatus("full", true, toNotCharging);
}

function toNotCharging() {
  changeStatus("not-charging", false, toUnknown);
}

function toUnknown() {
  changeStatus("unknown", false, cleanUp);
}

function cleanUp() {
  battery.onchargingchange = null;
  battery.onlevelchange = null;
  finish();
}

verifyInitialState();
