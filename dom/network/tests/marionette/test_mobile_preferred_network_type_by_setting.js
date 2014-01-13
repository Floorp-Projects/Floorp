/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const KEY = "ril.radio.preferredNetworkType";

SpecialPowers.addPermission("mobileconnection", true, document);
SpecialPowers.addPermission("settings-read", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let settings = window.navigator.mozSettings;

function test_revert_previous_setting_on_invalid_value() {
  log("Testing reverting to previous setting on invalid value received");

  let getLock = settings.createLock();
  let getReq = getLock.get(KEY);
  getReq.addEventListener("success", function onGetSuccess() {
    let originalValue = getReq.result[KEY] || "wcdma/gsm";

    let setDone = false;
    settings.addObserver(KEY, function observer(setting) {
      // Mark if the invalid value has been set in db and wait.
      if (setting.settingValue == obj[KEY]) {
        setDone = true;
        return;
      }

      // Skip any change before marking but keep it as original value.
      if (!setDone) {
        originalValue = setting.settingValue;
        return;
      }

      settings.removeObserver(KEY, observer);
      is(setting.settingValue, originalValue, "Settings reverted");
      window.setTimeout(cleanUp, 0);
    });

    let obj = {};
    obj[KEY] = "AnInvalidValue";
    let setLock = settings.createLock();
    setLock.set(obj);
    setLock.addEventListener("error", function onSetError() {
      ok(false, "cannot set '" + KEY + "'");
    });
  });
  getReq.addEventListener("error", function onGetError() {
    ok(false, "cannot get default value of '" + KEY + "'");
  });
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.removePermission("settings-write", document);
  SpecialPowers.removePermission("settings-read", document);

  finish();
}

waitFor(test_revert_previous_setting_on_invalid_value, function() {
  return navigator.mozMobileConnections[0].voice.connected;
});

