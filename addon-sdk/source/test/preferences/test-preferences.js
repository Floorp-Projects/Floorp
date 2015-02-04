/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var extend = require("lodash").extend

var e10sOff = require("./e10s-off.json");
var commonPrefs = require("./common.json");
var testPrefs = require("./test.json");
var fxPrefs = require("./firefox.json");
var disconnectionPrefs = require("./no-connections.json");

var prefs = extend({}, e10sOff, commonPrefs, testPrefs, disconnectionPrefs, fxPrefs);
module.exports = prefs;
