/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

var networkManager =
  Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
ok(networkManager,
   "networkManager.constructor is " + networkManager.constructor);

var wifiManager = window.navigator.mozWifiManager;
ok(wifiManager, "wifiManager.constructor is " + wifiManager.constructor);

function setEmulatorAPN() {
  let apn = [
    [{"carrier":"T-Mobile US",
      "apn":"epc.tmobile.com",
      "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
      "types":["default","supl","mms","ims","dun", "fota"]}]
  ];

  return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, apn);
}

function ensureWifiEnabled(aEnabled) {
  if (wifiManager.enabled === aEnabled) {
    log('Already ' + (aEnabled ? 'enabled' : 'disabled'));
    return Promise.resolve();
  }
  return requestWifiEnabled(aEnabled);
}

function requestWifiEnabled(aEnabled) {
  let promises = [];

  promises.push(waitForTargetEvent(wifiManager, aEnabled ? 'enabled' : 'disabled',
    function() {
      return wifiManager.enabled === aEnabled ? true : false;
  }));
  promises.push(setSettings(SETTINGS_KEY_WIFI_ENABLED, aEnabled));

  return Promise.all(promises);
}

// Test initial State
function verifyInitialState() {
  log("= verifyInitialState =");

  // Data and wifi should be off before starting any test.
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then(value => {
      is(value, false, "Data must be off");
    })
    .then(() => ensureWifiEnabled(false));
}

function testAllNetworkInfo(aAnyConnected) {
  log("= testAllNetworkInfo = " + aAnyConnected);

  let allNetworkInfo = networkManager.allNetworkInfo;
  ok(allNetworkInfo, "NetworkManager.allNetworkInfo");

  let count = Object.keys(allNetworkInfo).length;
  ok(count > 0, "NetworkManager.allNetworkInfo count");

  let connected = false;
  for (let networkId in allNetworkInfo) {
    if (allNetworkInfo.hasOwnProperty(networkId)) {
      let networkInfo = allNetworkInfo[networkId];
      if (networkInfo.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        connected = true;
        break;
      }
    }
  }

  is(aAnyConnected, connected, "NetworkManager.allNetworkInfo any connected");
}

// Start test
startTestBase(function() {

  let origApnSettings, origWifiEnabled;
  return Promise.resolve()
    .then(() => {
      origWifiEnabled = wifiManager.enabled;
    })
    .then(() => verifyInitialState())
    .then(() => getSettings(SETTINGS_KEY_DATA_APN_SETTINGS))
    .then(value => {
      origApnSettings = value;
    })
    .then(() => setEmulatorAPN())
    .then(() => setDataEnabledAndWait(true))
    .then(() => testAllNetworkInfo(true))
    .then(() => setDataEnabledAndWait(false))
    .then(() => testAllNetworkInfo(false))
    // Restore original apn settings and wifi state.
    .then(() => {
      if (origApnSettings) {
        return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, origApnSettings);
      }
    })
    .then(() => ensureWifiEnabled(origWifiEnabled));
});
