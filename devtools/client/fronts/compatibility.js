/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { compatibilitySpec } = require("devtools/shared/specs/compatibility");

class CompatibilityFront extends FrontClassWithSpec(compatibilitySpec) {}

exports.CompatibilityFront = CompatibilityFront;
registerFront(CompatibilityFront);
