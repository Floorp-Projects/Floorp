/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://devtools/client/shared/browser-loader.js");

// Initialize module loader and load all modules of the new inline
// preview feature. The entire code-base doesn't need any extra
// privileges and runs entirely in content scope.
const rootUrl = "resource://devtools/client/webconsole/new-console-output/";
const require = BrowserLoader({
  baseURI: rootUrl,
  window: this}).require;
const OutputWrapperThingy = require("./output-wrapper-thingy");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

this.NewConsoleOutput = function(parentNode) {
  console.log("Creating NewConsoleOutput", parentNode, OutputWrapperThingy);
  return new OutputWrapperThingy(parentNode);
}
