/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const TESTING_HOSTAPD = [{ ssid: 'ap0' }];

gTestSuite.doTestWithoutStockAp(function() {
  let firstNetwork;
  return gTestSuite.ensureWifiEnabled(true)
    // Start custom hostapd for testing.
    .then(() => gTestSuite.startHostapds(TESTING_HOSTAPD))
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', TESTING_HOSTAPD.length))

    // Request the first scan.
    .then(gTestSuite.requestWifiScan)
    .then(function(networks) {
      firstNetwork = networks[0];
      return gTestSuite.testAssociate(firstNetwork);
    })

    // Note that due to Bug 1168285, we need to re-start testing hostapd
    // after wifi has been re-enabled.

    // Disable wifi and kill running hostapd.
    .then(() => gTestSuite.requestWifiEnabled(false))
    .then(gTestSuite.killAllHostapd)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', 0))

    // Re-enable wifi.
    .then(() => gTestSuite.requestWifiEnabled(true))

    // Restart hostapd.
    .then(() => gTestSuite.startHostapds(TESTING_HOSTAPD))
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', TESTING_HOSTAPD.length))

    // Wait for connection automatically.
    .then(() => gTestSuite.waitForConnected(firstNetwork))

    // Kill running hostapd.
    .then(gTestSuite.killAllHostapd)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', 0))
});
