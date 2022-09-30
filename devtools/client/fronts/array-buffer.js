/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  arrayBufferSpec,
} = require("resource://devtools/shared/specs/array-buffer.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

/**
 * A ArrayBufferClient provides a way to access ArrayBuffer from the
 * devtools server.
 */
class ArrayBufferFront extends FrontClassWithSpec(arrayBufferSpec) {
  form(json) {
    this.length = json.length;
  }
}

exports.ArrayBufferFront = ArrayBufferFront;
registerFront(ArrayBufferFront);
