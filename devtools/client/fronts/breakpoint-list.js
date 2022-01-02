/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { breakpointListSpec } = require("devtools/shared/specs/breakpoint-list");

class BreakpointListFront extends FrontClassWithSpec(breakpointListSpec) {}

registerFront(BreakpointListFront);
