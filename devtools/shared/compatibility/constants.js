/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file might be required from a node script (./bin/update.js), so don't use
// Chrome API here.

const TARGET_BROWSER_ID = [
  "firefox",
  "firefox_android",
  "chrome",
  "chrome_android",
  "safari",
  "safari_ios",
  "edge",
  "ie",
];
const TARGET_BROWSER_STATUS = ["esr", "current", "beta", "nightly"];
const TARGET_BROWSER_PREF = "devtools.inspector.compatibility.target-browsers";

module.exports = {
  TARGET_BROWSER_ID,
  TARGET_BROWSER_PREF,
  TARGET_BROWSER_STATUS,
};
