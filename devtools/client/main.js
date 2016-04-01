/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module could have been devtools-browser.js.
 * But we need this wrapper in order to define precisely what we are exporting
 * out of client module loader (Loader.jsm): only Toolbox and TargetFactory.
 */

// For compatiblity reasons, exposes these symbols on "devtools":
Object.defineProperty(exports, "Toolbox", {
  get: () => require("devtools/client/framework/toolbox").Toolbox
});
Object.defineProperty(exports, "TargetFactory", {
  get: () => require("devtools/client/framework/target").TargetFactory
});

// Load the main browser module
require("devtools/client/framework/devtools-browser");
