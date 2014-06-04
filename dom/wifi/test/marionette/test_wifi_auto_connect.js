/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.doTest(function() {
  let firstNetwork;
  return gTestSuite.ensureWifiEnabled(true)
    .then(gTestSuite.requestWifiScan)
    .then(function(networks) {
      firstNetwork = networks[0];
      return gTestSuite.testAssociate(firstNetwork);
    })
    .then(() => gTestSuite.requestWifiEnabled(false))
    .then(() => gTestSuite.requestWifiEnabled(true))
    .then(() => gTestSuite.waitForConnected(firstNetwork));
});
