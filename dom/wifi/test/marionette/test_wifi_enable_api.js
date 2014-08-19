/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.doTest(function() {
  return Promise.resolve()
    .then(() => gTestSuite.ensureWifiEnabled(false, true))
    .then(() => gTestSuite.requestWifiEnabled(true, true))
    .then(() => gTestSuite.requestWifiEnabled(false, true))
    .then(() => gTestSuite.ensureWifiEnabled(true, true));
});
