/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */

/* exported CommonUtils, testChildAtPoint, Layout, hitTest */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

const { CommonUtils } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
);

const { Layout } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
);
