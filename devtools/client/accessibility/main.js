/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { utils: Cu } = Components;
const { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});

// Module Loader
const require = BrowserLoader({
  baseURI: "resource://devtools/client/accessibility/",
  window,
}).require;

// Load accessibility panel content
require("./accessibility-view.js");
