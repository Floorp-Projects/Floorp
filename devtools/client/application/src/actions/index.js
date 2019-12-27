/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const workers = require("devtools/client/application/src/actions/workers");
const page = require("devtools/client/application/src/actions/page");
const ui = require("devtools/client/application/src/actions/ui");
const manifest = require("devtools/client/application/src/actions/manifest");

Object.assign(exports, workers, page, ui, manifest);
