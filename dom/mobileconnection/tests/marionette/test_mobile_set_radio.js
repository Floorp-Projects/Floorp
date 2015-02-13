/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testAutoRestoreRadioState(aEnabled, aServiceId) {
  return setRadioEnabledAndWait(aEnabled, aServiceId)
    .then(() => {
      let expectedSequence = aEnabled ? ["enabling", "enabled"] :
                                        ["disabling", "disabled"];

      let p1 = waitForManagerEvent("radiostatechange", aServiceId, function() {
        let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
        let expectedRadioState = expectedSequence.shift();
        is(mobileConn.radioState, expectedRadioState, "Check radio state");
        return expectedSequence.length === 0;
      });

      let command = "modem radio " + (aEnabled ? "disable" : "enable");
      let p2 = runEmulatorCmdSafe(command);

      return Promise.all([p1, p2]);
    });
}

// Start tests
startTestCommon(function() {

  let origApnSettings;
  return getDataApnSettings()
    .then(value => {
      origApnSettings = value;
    })

    // Test disabling/enabling radio power.
    .then(() => log("Test disabling/enabling radio power"))
    .then(() => setRadioEnabledAndWait(false))
    .then(() => setRadioEnabledAndWait(true))

    // Test auto-restore radio state
    .then(() => log("Test auto-restore radio state"))
    .then(() => testAutoRestoreRadioState(false))
    .then(() => testAutoRestoreRadioState(true))

    // Test disabling radio when data is connected.
    .then(() => log("Test disabling radio when data is connected"))
    .then(() => {
      let apnSettings = [[
        {"carrier":"T-Mobile US",
         "apn":"epc.tmobile.com",
         "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
         "types":["default","supl","mms"]}]];
      return setDataApnSettings(apnSettings);
    })
    .then(() => setDataEnabledAndWait(true))
    .then(() => setRadioEnabledAndWait(false))
    .then(() => {
      // Data should be disconnected.
      is(mobileConnection.data.connected, false);
    })

    // Restore test environment.
    .then(() => log("Restore test environment"))
    .then(() => setDataApnSettings(origApnSettings))
    .then(() => setDataEnabled(false))
    .then(() => setRadioEnabledAndWait(true));

}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
