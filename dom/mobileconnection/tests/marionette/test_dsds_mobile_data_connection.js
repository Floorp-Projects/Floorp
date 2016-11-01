/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const SETTINGS_KEY_DATA_DEFAULT_ID = "ril.data.defaultServiceId";

var connections;
var numOfRadioInterfaces;
var currentDataDefaultId = 0;

function muxModem(id) {
  return runEmulatorCmdSafe("mux modem " + id);
}

function switchDataDefaultId(defaultId) {
  return Promise.resolve()
    .then(() => setSettings1(SETTINGS_KEY_DATA_DEFAULT_ID, defaultId))
    .then(() => {
      log("Data default id switched to: " + defaultId);
      currentDataDefaultId = defaultId;
    });
}

function setDataRoamingSettings(enableDataRoammingIds) {
  let dataRoamingSettings = [];
  for (let i = 0; i < numOfRadioInterfaces; i++) {
    dataRoamingSettings.push(false);
  }

  for (let i = 0; i < enableDataRoammingIds.length; i++) {
    log("Enable data roaming for id: " + enableDataRoammingIds[i]);
    dataRoamingSettings[enableDataRoammingIds[i]] = true;
  }

  return setDataRoamingEnabled(dataRoamingSettings);
}

function setApnSettings() {
  let apn = [{
    "carrier":"T-Mobile US",
    "apn":"epc.tmobile.com",
    "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
    "types":["default","supl","mms"]
  }];

  // Use the same APN for all sims for now.
  let apns = [];
  for (let i = 0; i < numOfRadioInterfaces; i++) {
    apns.push(apn);
  }

  return setDataApnSettings(apns);

}

function waitForDataState(clientId, connected) {
  let deferred = Promise.defer();

  let connection = connections[clientId];
  if (connection.data.connected === connected) {
    log("data connection for client " + clientId + " is now " +
      connection.data.connected);
    deferred.resolve();
    return;
  }

  return Promise.resolve()
    .then(() => waitForManagerEvent("datachange", clientId))
    .then(() => waitForDataState(clientId, connected));
}

function restoreTestEnvironment() {
  return Promise.resolve()
    .then(() => setEmulatorRoamingAndWait(false, currentDataDefaultId))
    .then(() => setDataRoamingSettings([]))
    .then(() => switchDataDefaultId(0))
    .then(() => muxModem(0));
}

function testEnableData() {
  log("Turn data on.");

  let connection = connections[currentDataDefaultId];
  is(connection.data.connected, false);

  return Promise.resolve()
    .then(() => setApnSettings())
    .then(() => setDataEnabledAndWait(true, currentDataDefaultId));
}

function testSwitchDefaultDataToSimTwo() {
  log("Switch data connection to sim 2.");

  is(currentDataDefaultId, 0);
  let connection = connections[currentDataDefaultId];
  is(connection.data.connected, true);

  return Promise.resolve()
    .then(() => switchDataDefaultId(1))
    .then(() => {
      is(currentDataDefaultId, 1);
    })
    .then(() => waitForDataState(currentDataDefaultId, true));
}

function testDisableDataRoamingWhileRoaming() {
  log("Disable data roaming setting while roaming.");

  let connection = connections[currentDataDefaultId];
  is(connection.data.connected, true);
  is(connection.data.roaming, false);

  return Promise.resolve()
    .then(() => setDataRoamingSettings([]))
    .then(() => muxModem(currentDataDefaultId))
    .then(() => setEmulatorRoamingAndWait(true, currentDataDefaultId))
    .then(() => waitForDataState(currentDataDefaultId, false));
}

function testEnableDataRoamingWhileRoaming() {
  log("Enable data roaming setting while roaming.");

  let connection = connections[currentDataDefaultId];
  is(connection.data.connected, false);
  is(connection.data.roaming, true);

  return Promise.resolve()
    .then(() => setDataRoamingSettings([currentDataDefaultId]))
    .then(() => waitForDataState(currentDataDefaultId, true));
}

function testDisableData() {
  log("Turn data off.");

  let connection = connections[currentDataDefaultId];
  is(connection.data.connected, true);

  return Promise.resolve()
    .then(() => setDataEnabledAndWait(false, currentDataDefaultId));
}

startDSDSTestCommon(function() {
  connections = workingFrame.contentWindow.navigator.mozMobileConnections;
  numOfRadioInterfaces = getNumOfRadioInterfaces();

  return testEnableData()
    .then(testSwitchDefaultDataToSimTwo)
    .then(testDisableDataRoamingWhileRoaming)
    .then(testEnableDataRoamingWhileRoaming)
    .then(testDisableData)
    .then(restoreTestEnvironment);
}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
