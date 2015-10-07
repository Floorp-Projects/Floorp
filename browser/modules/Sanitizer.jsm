/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

//
// A shared module for sanitize.js
//
// Until bug 1167238 lands, this serves only as a way to ensure that
// sanitize is loaded from its own compartment, rather than from that
// of the sanitize dialog.
//

this.EXPORTED_SYMBOLS = ["Sanitizer"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var scope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
 .loadSubScript("chrome://browser/content/sanitize.js", scope);

this.Sanitizer = scope.Sanitizer;
