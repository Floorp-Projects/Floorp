/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let battery = window.navigator.battery;

function verifyInitialState() {
  ok(battery, "battery");
  is(battery.level, 0.5, "battery.level");
  runEmulatorCmd("power display", function (result) {
    is(result.pop(), "OK", "power display successful");
    ok(result.indexOf("capacity: 50") !== -1, "power capacity");
    setUp();
  });
}

function unexpectedEvent(event) {
  ok(false, "Unexpected " + event.type + " event");
}

function setUp() {
  battery.onchargingchange = unexpectedEvent;
  battery.onlevelchange = unexpectedEvent;
  levelUp();
}

function changeCapacity(capacity, changeExpected, nextFunction) {
  log("Changing power capacity to " + capacity);
  if (changeExpected) {
    battery.onlevelchange = function (event) {
     battery.onlevelchange = unexpectedEvent;
     is(event.type, "levelchange", "event.type");
     is(battery.level, capacity / 100, "battery.level");
     nextFunction();
    };
    runEmulatorCmd("power capacity " + capacity);
  }
  else {
    runEmulatorCmd("power capacity " + capacity, function () {
      is(battery.level, capacity / 100, "battery.level");
      nextFunction();
    });
  }
}

function levelUp() {
  changeCapacity("90", true, levelDown);
}

function levelDown() {
  changeCapacity("10", true, levelSame);
}

function levelSame() {
  changeCapacity("10", false, cleanUp);
}

function cleanUp() {
  battery.onchargingchange = null;
  battery.onlevelchange = function () {
    battery.onlevelchange = null;
    finish();
  };
  runEmulatorCmd("power capacity 50");
}

verifyInitialState();
