/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const workers = require("resource://devtools/client/application/src/actions/workers.js");
const page = require("resource://devtools/client/application/src/actions/page.js");
const ui = require("resource://devtools/client/application/src/actions/ui.js");
const manifest = require("resource://devtools/client/application/src/actions/manifest.js");

Object.assign(exports, workers, page, ui, manifest);
