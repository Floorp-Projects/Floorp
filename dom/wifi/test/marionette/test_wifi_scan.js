/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SCAN_RETRY_CNT = 5;

/**
 * Test scan with no AP present.
 *
 * The precondition is:
 *   1) Wifi is enabled.
 *   2) All the hostapds are turned off.
 *
 * @return deferred promise.
 */
function testScanNoAp() {
  return gTestSuite.testWifiScanWithRetry(SCAN_RETRY_CNT, []);
}

/**
 * Test scan with APs present.
 *
 * The precondition is:
 *   1) Wifi is enabled.
 *   2) All the hostapds are turned off.
 *
 * @return deferred promise.
 */
function testScanWithAps() {
  return gTestSuite.startHostapds(HOSTAPD_CONFIG_LIST)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', HOSTAPD_CONFIG_LIST.length))
    .then(() => gTestSuite.testWifiScanWithRetry(SCAN_RETRY_CNT, HOSTAPD_CONFIG_LIST))
    .then(gTestSuite.killAllHostapd)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', 0));
}

gTestSuite.doTestWithoutStockAp(function() {
  return gTestSuite.ensureWifiEnabled(true)
    .then(testScanNoAp)
    .then(testScanWithAps);
});
