/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports["test traits from objects"] = require("./traits/object-tests");
exports["test traits from descriptors"] = require("./traits/descriptor-tests");
exports["test inheritance"] = require("./traits/inheritance-tests");

require("test").run(exports);
