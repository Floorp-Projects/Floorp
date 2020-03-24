/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(() => {
  // Always clean up the prefs after every test.
  const { revertRecordingPreferences } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );
  revertRecordingPreferences();
});
