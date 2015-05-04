/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

gTestSuite.startTest(function() {
  let origApnSettings;
  return gTestSuite.getDataApnSettings()
    .then(value => {
      origApnSettings = value;
    })
    .then(() => {
      // Set dun apn settings.
      let apnSettings = [[ { "carrier": "T-Mobile US",
                             "apn": "epc1.tmobile.com",
                             "mmsc": "http://mms.msg.eng.t-mobile.com/mms/wapenc",
                             "types": ["default","supl","mms"] },
                           { "carrier": "T-Mobile US",
                             "apn": "epc2.tmobile.com",
                             "types": ["dun"] } ]];
      return gTestSuite.setDataApnSettings(apnSettings);
    })
    .then(() => gTestSuite.setTetheringDunRequired())
    .then(() => gTestSuite.startTetheringTest(function() {
      return gTestSuite.ensureWifiEnabled(false)
        .then(() => gTestSuite.setWifiTetheringEnabled(true, true))
        .then(() => gTestSuite.setWifiTetheringEnabled(false, true));
    }))
    // Restore apn settings.
    .then(() => {
      if (origApnSettings) {
        return gTestSuite.setDataApnSettings(origApnSettings);
      }
    });
});