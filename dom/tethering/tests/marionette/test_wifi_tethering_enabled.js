/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.startTetheringTest(function() {
  return gTestSuite.ensureWifiEnabled(false)
    .then(() => gTestSuite.setWifiTetheringEnabled(true))
    .then(() => gTestSuite.setWifiTetheringEnabled(false));
});
