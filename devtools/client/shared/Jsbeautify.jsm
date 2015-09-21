/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * JS Beautifier. Please use require("devtools/jsbeautify") instead of
 * this JSM.
 */

this.EXPORTED_SYMBOLS = [ "jsBeautify" ];

const { require } = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
const { beautify } = require("devtools/jsbeautify");
const jsBeautify = beautify.js;
