/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function checkOrWaitForDataState(connected) {
  if (mobileConnection.data.connected == connected) {
    log("data.connected is now " + mobileConnection.data.connected);
    return;
  }

  return waitForManagerEvent("datachange")
    .then(() => checkOrWaitForDataState(connected));
}

function verifyInitialState() {
  log("Verifying initial state.");

  // Data should be off and registration home before starting any test.
  return Promise.resolve()
    .then(function() {
      is(mobileConnection.voice.state, "registered", "voice.state");
      is(mobileConnection.data.state, "registered", "data.state");
      is(mobileConnection.voice.roaming, false, "voice.roaming");
      is(mobileConnection.data.roaming, false, "data.roaming");
    })
    .then(getDataEnabled)
    .then(function(aResult) {
      is(aResult, false, "Data must be off.")
    });
}

function testEnableData() {
  log("Turn data on.");

  return setDataEnabledAndWait(true);
}

function testUnregisterDataWhileDataEnabled() {
  log("Set data registration unregistered while data enabled.");

  // When data registration is unregistered, all data calls will be
  // automatically deactivated.
  return setEmulatorVoiceDataStateAndWait("data", "unregistered")
    .then(() => checkOrWaitForDataState(false));
}

function testRegisterDataWhileDataEnabled() {
  log("Set data registration home while data enabled.");

  // When data registration is registered, data call will be (re)activated by
  // gecko if ril.data.enabled is set to true.
  return setEmulatorVoiceDataStateAndWait("data", "home")
    .then(() => checkOrWaitForDataState(true));
}

function testDisableDataRoamingWhileRoaming() {
  log("Disable data roaming while roaming.");

  // After setting emulator state to roaming, data connection should be
  // disconnected due to data roaming setting set to off.
  return setEmulatorRoamingAndWait(true)
    .then(() => checkOrWaitForDataState(false));
}

function testEnableDataRoamingWhileRoaming() {
  log("Enable data roaming while roaming.");

  // Data should be re-connected as we enabled data roaming.
  return setDataRoamingEnabled(true)
    .then(() => checkOrWaitForDataState(true));
}

function testDisableData() {
  log("Turn data off.");

  return setDataEnabledAndWait(false);
}

startTestCommon(function() {

  let origApnSettings;
  return verifyInitialState()
    .then(() => getDataApnSettings())
    .then(value => {
      origApnSettings = value;
    })
    .then(() => {
      let apnSettings = [[{ "carrier": "T-Mobile US",
                            "apn": "epc.tmobile.com",
                            "mmsc": "http://mms.msg.eng.t-mobile.com/mms/wapenc",
                            "types": ["default","supl","mms"] }]];
      return setDataApnSettings(apnSettings);
    })
    .then(() => testEnableData())
    .then(() => testUnregisterDataWhileDataEnabled())
    .then(() => testRegisterDataWhileDataEnabled())
    .then(() => testDisableDataRoamingWhileRoaming())
    .then(() => testEnableDataRoamingWhileRoaming())
    .then(() => testDisableData())
    // Restore test environment.
    .then(() => {
      if (origApnSettings) {
        return setDataApnSettings(origApnSettings);
      }
    })
    .then(() => setEmulatorRoamingAndWait(false))
    .then(() => setDataRoamingEnabled(false));

}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);