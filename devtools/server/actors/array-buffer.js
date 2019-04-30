/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var protocol = require("devtools/shared/protocol");
const {arrayBufferSpec} = require("devtools/shared/specs/array-buffer");

/**
 * Creates an actor for the specified ArrayBuffer.
 *
 * @param {DebuggerServerConnection} conn
 *    The server connection.
 * @param buffer ArrayBuffer
 *        The buffer.
 */
const ArrayBufferActor = protocol.ActorClassWithSpec(arrayBufferSpec, {
  initialize: function(conn, buffer) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.buffer = buffer;
    this.bufferLength = buffer.byteLength;
  },

  rawValue: function() {
    return this.buffer;
  },

  form: function() {
    return {
      typeName: this.typeName,
      length: this.bufferLength,
      actor: this.actorID,
    };
  },

  slice(start, count) {
    const slice = new Uint8Array(this.buffer, start, count);
    const parts = [];
    let offset = 0;
    const PortionSize = 0x6000; // keep it divisible by 3 for btoa() and join()
    while (offset + PortionSize < count) {
      parts.push(btoa(
        String.fromCharCode.apply(null, slice.subarray(offset, offset + PortionSize))));
      offset += PortionSize;
    }
    parts.push(btoa(String.fromCharCode.apply(null, slice.subarray(offset, count))));
    return {
      "from": this.actorID,
      "encoded": parts.join(""),
    };
  },
});

module.exports = {
  ArrayBufferActor,
};
