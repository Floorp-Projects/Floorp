/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.doTestTethering(function() {
  return gTestSuite.ensureWifiEnabled(true)
    .then(function() {
      return Promise.all([
        gTestSuite.waitForWifiManagerEventOnce('disabled'),
        gTestSuite.requestTetheringEnabled(true)
      ]);
    })
    .then(function() {
      return Promise.all([
        gTestSuite.waitForWifiManagerEventOnce('enabled'),
        gTestSuite.requestTetheringEnabled(false)
      ]);
    });
});
