/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

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
        gTestSuite.waitForRilDataConnected(false),
        gTestSuite.waitForConnected(firstNetwork),

        // 2) Stop wifi tethering.
        gTestSuite.requestTetheringEnabled(false)
      ]);
    })
    // Remove wlan0 from default route by disabling wifi. Otherwise,
    // it will cause the subsequent test cases loading page error.
    .then(() => gTestSuite.requestWifiEnabled(false));
});