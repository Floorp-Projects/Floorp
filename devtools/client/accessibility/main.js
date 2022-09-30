/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

// Module Loader
const require = BrowserLoader({
  baseURI: "resource://devtools/client/accessibility/",
  window,
}).require;

// Load accessibility panel content
require("resource://devtools/client/accessibility/accessibility-view.js");
