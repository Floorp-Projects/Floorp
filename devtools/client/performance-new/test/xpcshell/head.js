/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

registerCleanupFunction(() => {
  // Always clean up the prefs after every test.
  const { revertRecordingSettings } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );
  revertRecordingSettings();
});
