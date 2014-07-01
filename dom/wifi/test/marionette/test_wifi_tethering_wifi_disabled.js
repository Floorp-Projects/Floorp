/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.doTestTethering(function() {
  return gTestSuite.ensureWifiEnabled(false)
    .then(() => gTestSuite.requestTetheringEnabled(true))
    .then(() => gTestSuite.requestTetheringEnabled(false))
});
