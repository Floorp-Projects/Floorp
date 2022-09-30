/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const debugTargets = require("resource://devtools/client/aboutdebugging/src/actions/debug-targets.js");
const runtimes = require("resource://devtools/client/aboutdebugging/src/actions/runtimes.js");
const telemetry = require("resource://devtools/client/aboutdebugging/src/actions/telemetry.js");
const ui = require("resource://devtools/client/aboutdebugging/src/actions/ui.js");

Object.assign(exports, ui, runtimes, telemetry, debugTargets);
