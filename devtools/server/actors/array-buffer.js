/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var protocol = require("devtools/shared/protocol");
const { arrayBufferSpec } = require("devtools/shared/specs/array-buffer");

/**
 * Creates an actor for the specified ArrayBuffer.
 *
 * @param {DevToolsServerConnection} conn
 *    The server connection.
 * @param buffer ArrayBuffer
 *        The buffer.
 */
const ArrayBufferActor = protocol.ActorClassWithSpec(arrayBufferSpec, {
  initialize(conn, buffer) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.buffer = buffer;
    this.bufferLength = buffer.byteLength;
  },

  rawValue() {
    return this.buffer;
  },

  form() {
    return {
      actor: this.actorID,
      length: this.bufferLength,
      // The `typeName` is read in the source spec when reading "sourcedata"
      // which can either be an ArrayBuffer actor or a LongString actor.
      typeName: this.typeName,
    };
  },

  slice(start, count) {
    const slice = new Uint8Array(this.buffer, start, count);
    const parts = [];
    let offset = 0;
    const PortionSize = 0x6000; // keep it divisible by 3 for btoa() and join()
    while (offset + PortionSize < count) {
      parts.push(
        btoa(
          String.fromCharCode.apply(
            null,
            slice.subarray(offset, offset + PortionSize)
          )
        )
      );
      offset += PortionSize;
    }
    parts.push(
      btoa(String.fromCharCode.apply(null, slice.subarray(offset, count)))
    );
    return {
      from: this.actorID,
      encoded: parts.join(""),
    };
  },
});

module.exports = {
  ArrayBufferActor,
};
