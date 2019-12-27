/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const debugTargets = require("./debug-targets");
const runtimes = require("./runtimes");
const telemetry = require("./telemetry");
const ui = require("./ui");

Object.assign(exports, ui, runtimes, telemetry, debugTargets);
