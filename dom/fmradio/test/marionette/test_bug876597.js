/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("fmradio", true, document);
SpecialPowers.addPermission("settings-read", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let FMRadio = window.navigator.mozFMRadio;
let mozSettings = window.navigator.mozSettings;
let KEY = "airplaneMode.enabled";

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio);
  is(FMRadio.enabled, false);
  ok(mozSettings);

  checkAirplaneModeSettings();
}

function checkAirplaneModeSettings() {
  log("Checking airplane mode settings");
  let req = mozSettings.createLock().get(KEY);
  req.onsuccess = function(event) {
    ok(!req.result[KEY], "Airplane mode is disabled.");
    enableFMRadio();
  };

  req.onerror = function() {
    ok(false, "Error occurs when reading settings value.");
    finish();
  };
}

function enableFMRadio() {
  log("Enable FM radio");
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  let req = FMRadio.enable(frequency);

  req.onsuccess = function() {
    enableAirplaneMode();
  };

  req.onerror = function() {
    ok(false, "Failed to enable FM radio.");
  };
}

function enableAirplaneMode() {
  log("Enable airplane mode");
  FMRadio.ondisabled = function() {
    FMRadio.ondisabled = null;
    enableFMRadioWithAirplaneModeEnabled();
  };

  let settings = {};
  settings[KEY] = true;
  mozSettings.createLock().set(settings);
}

function enableFMRadioWithAirplaneModeEnabled() {
  log("Enable FM radio with airplane mode enabled");
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  let req = FMRadio.enable(frequency);
  req.onerror = cleanUp();

  req.onsuccess = function() {
    ok(false, "FMRadio could be enabled when airplane mode is enabled.");
  };
}

function cleanUp() {
  let settings = {};
  settings[KEY] = false;
  let req = mozSettings.createLock().set(settings);

  req.onsuccess = function() {
    ok(!FMRadio.enabled);
    finish();
  };

  req.onerror = function() {
    ok(false, "Error occurs when setting value");
  };
}

verifyInitialState();

