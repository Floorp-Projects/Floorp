/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const TESTING_HOSTAPD = [{ ssid: 'ap0' }];

function connectToFirstNetwork() {
  let firstNetwork;
  return gTestSuite.requestWifiScan()
    .then(function (networks) {
      firstNetwork = networks[0];
      return gTestSuite.testAssociate(firstNetwork);
    })
    .then(() => firstNetwork);
}

gTestSuite.doTestTethering(function() {
  let firstNetwork;

  return gTestSuite.ensureWifiEnabled(true)
    // Start custom hostapd for testing.
    .then(() => gTestSuite.startHostapds(TESTING_HOSTAPD))
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', TESTING_HOSTAPD.length))

    // Connect to the testing AP and wait for data becoming disconnected.
    .then(function () {
      return Promise.all([
        // 1) Set up the event listener first:
        //    RIL data should become disconnected after connecting to wifi.
        gTestSuite.waitForRilDataConnected(false),

        // 2) Connect to the first scanned network.
        connectToFirstNetwork()
          .then(aFirstNetwork => firstNetwork = aFirstNetwork)
      ]);
    })
    .then(function() {
      return Promise.all([
        // 1) Set up the event listeners first:
        //    Wifi should be turned off and RIL data should be connected.
        gTestSuite.waitForWifiManagerEventOnce('disabled'),
        gTestSuite.waitForRilDataConnected(true),

        // 2) Start wifi tethering.
        gTestSuite.requestTetheringEnabled(true)
      ]);
    })
    .then(function() {
      return Promise.all([
        // 1) Set up the event listeners first:
        //    Wifi should be enabled, RIL data should become disconnected and
        //    we should connect to an wifi AP.
        gTestSuite.waitForWifiManagerEventOnce('enabled'),

        // Due to Bug 1168285, after re-enablin wifi, the remembered network
        // will not be connected automatically. Leave "auto connect test"
        // covered by test_wifi_auto_connect.js.
        //gTestSuite.waitForRilDataConnected(false),
        //gTestSuite.waitForConnected(firstNetwork),

        // 2) Stop wifi tethering.
        gTestSuite.requestTetheringEnabled(false)
      ]);
    })
    // Remove wlan0 from default route by disabling wifi. Otherwise,
    // it will cause the subsequent test cases loading page error.
    .then(() => gTestSuite.requestWifiEnabled(false))

    // Kill running hostapd.
    .then(gTestSuite.killAllHostapd)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', 0));
});