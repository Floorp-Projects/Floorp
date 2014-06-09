/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// Must sync with hardware/ril/reference-ril/reference-ril.c
const MAX_DATA_CONTEXTS = 4;

function setEmulatorAPN() {
  // Use different apn for each network type.
  let apn = [[ { "carrier":"T-Mobile US",
                 "apn":"epc1.tmobile.com",
                 "types":["default"] },
               { "carrier":"T-Mobile US",
                 "apn":"epc2.tmobile.com",
                 "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
                 "types":["mms"] },
               { "carrier":"T-Mobile US",
                 "apn":"epc3.tmobile.com",
                 "types":["supl"] },
               { "carrier":"T-Mobile US",
                 "apn":"epc4.tmobile.com",
                 "types":["ims"] },
               { "carrier":"T-Mobile US",
                 "apn":"epc5.tmobile.com",
                 "types":["dun"] }]];

  return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, apn);
}

// Test initial State
function testInitialState() {
  log("= testInitialState =");

  // Data should be off before starting any test.
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then(value => {
      is(value, false, "Data must be off");
    });
}

function testSetupConcurrentDataCalls() {
  log("= testSetupConcurrentDataCalls =");

  let promise = Promise.resolve();
  let types = Object.keys(mobileTypeMapping);
  // Skip default mobile type.
  for (let i = 1; i < MAX_DATA_CONTEXTS; i++) {
    let type = types[i];
    promise = promise.then(() => setupDataCallAndWait(type));
  }
  return promise;
}

function testDeactivateConcurrentDataCalls() {
  log("= testDeactivateConcurrentDataCalls =");

  let promise = Promise.resolve();
  let types = Object.keys(mobileTypeMapping);
  // Skip default mobile type.
  for (let i = 1; i < MAX_DATA_CONTEXTS; i++) {
    let type = types[i];
    promise = promise.then(() => deactivateDataCallAndWait(type));
  }
  return promise;
}

// Start test
startTestBase(function() {

  let origApnSettings;
  return testInitialState()
    .then(() => getSettings(SETTINGS_KEY_DATA_APN_SETTINGS))
    .then(value => {
      origApnSettings = value;
    })
    .then(() => setEmulatorAPN())
    .then(() => setDataEnabledAndWait(true))
    .then(() => testSetupConcurrentDataCalls())
    .then(() => testDeactivateConcurrentDataCalls())
    .then(() => setDataEnabledAndWait(false))
    .then(() => {
      if (origApnSettings) {
        return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, origApnSettings);
      }
    });
});
