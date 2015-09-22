/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * JS Beautifier. Please use require("devtools/shared/jsbeautify/beautify") instead of
 * this JSM.
 */

this.EXPORTED_SYMBOLS = [ "jsBeautify" ];

const { require } = Components.utils.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
const { beautify } = require("devtools/shared/jsbeautify/beautify");
const jsBeautify = beautify.js;
