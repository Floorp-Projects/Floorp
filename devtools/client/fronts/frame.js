/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { frameSpec } = require("resource://devtools/shared/specs/frame.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class FrameFront extends FrontClassWithSpec(frameSpec) {
  form(json) {
    this.displayName = json.displayName;
    this.arguments = json.arguments;
    this.type = json.type;
    this.where = json.where;
    this.this = json.this;
    this.data = json;
    this.asyncCause = json.asyncCause;
    this.state = json.state;
  }
}

module.exports = FrameFront;
registerFront(FrameFront);
