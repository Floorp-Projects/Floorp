/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let battery = window.navigator.battery;
ok(battery);

/**
 * Helper that compares the emulator state against the web API.
 */
function verifyAPI(callback) {
  runEmulatorCmd("power display", function(result) {
    log("Power status: " + result);
    is(result.pop(), "OK");

    let power = {};
    for (let i = 0; i < result.length; i++) {
      let [key, value] = result[i].split(": ");
      power[key.toLowerCase()] = value.toLowerCase();
    }
    power.capacity = parseInt(power.capacity, 10);

    is(battery.charging, power.status == "charging" || power.status == "full",
       "Charging state matches '" + power.status + "'.");
    is(battery.level, power.capacity / 100, "Capacity matches.");

    // At this time there doesn't seem to be a way to emulate
    // charging/discharging time.
    is(battery.dischargingTime, Infinity);
    is(battery.chargingTime, Infinity);

    window.setTimeout(callback, 0);
  });
}

function unexpectedEvent(event) {
  log("Unexpected " + event.type + " event");
  ok(false, "Unexpected " + event.type + " event");
}

/**
 * Test story begins here.
 */

function setup() {
  log("Providing initial setup: charging the battery.");

  runEmulatorCmd("power status charging", function(result) {
    battery.onchargingchange = unexpectedEvent;
    battery.onlevelchange = unexpectedEvent;
    verifyAPI(full);
  });
}

function full() {
  log("Switching the battery state to full. No changes expected.");

  runEmulatorCmd("power status full", function(result) {
    verifyAPI(notcharging);
  });
}

function notcharging() {
  log("Switching the battery state to not charging.");

  runEmulatorCmd("power status not-charging");
  battery.onchargingchange = function onchargingchange(event) {
    battery.onchargingchange = unexpectedEvent;
    verifyAPI(discharging);
  };
}

function discharging() {
  log("Discharging the battery.");

  runEmulatorCmd("power status discharging", function(result) {
    verifyAPI(level);
  });
}

function level() {
  log("Changing the charge level.");

  let newCapacity = battery.level * 100 / 2;
  runEmulatorCmd("power capacity " + newCapacity);

  battery.onlevelchange = function onlevelchange(event) {
    battery.onlevelchange = unexpectedEvent;
    verifyAPI(charging);
  };
}

function charging() {
  log("Charging the battery.");

  runEmulatorCmd("power status charging");
  battery.onchargingchange = function onchargingchange(event) {
    battery.onchargingchange = unexpectedEvent;
    verifyAPI(cleanup);
  };  
}

function cleanup() {
  battery.onchargingchange = null;
  battery.onlevelchange = null;
  finish();
}

setup();
