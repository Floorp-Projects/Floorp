/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

var { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
var { BrowserLoader } = ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
var DevToolsUtils = require("devtools/shared/DevToolsUtils");

var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window
});

window.EVENTS = {};
window.on = function() {};
window.off = function() {};

SimpleTest.registerCleanupFunction(() => {
  window.EVENTS = null;
  window.on = null;
  window.off = null;
});

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();
