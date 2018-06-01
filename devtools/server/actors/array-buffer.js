/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Creates an actor for the specified ArrayBuffer.
 *
 * @param buffer ArrayBuffer
 *        The buffer.
 */
function ArrayBufferActor(buffer) {
  this.buffer = buffer;
  this.bufferLength = buffer.byteLength;
}

ArrayBufferActor.prototype = {
  actorPrefix: "arrayBuffer",

  rawValue: function() {
    return this.buffer;
  },

  destroy: function() {
  },

  grip() {
    return {
      "type": "arrayBuffer",
      "length": this.bufferLength,
      "actor": this.actorID
    };
  },

  onSlice({start, count}) {
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
  }
};

ArrayBufferActor.prototype.requestTypes = {
  "slice": ArrayBufferActor.prototype.onSlice,
};

/**
 * Create a grip for the given ArrayBuffer.
 *
 * @param buffer ArrayBuffer
 *        The ArrayBuffer we are creating a grip for.
 * @param pool ActorPool
 *        The actor pool where the new actor will be added.
 */
function arrayBufferGrip(buffer, pool) {
  if (!pool.arrayBufferActors) {
    pool.arrayBufferActors = new WeakMap();
  }

  if (pool.arrayBufferActors.has(buffer)) {
    return pool.arrayBufferActors.get(buffer).grip();
  }

  const actor = new ArrayBufferActor(buffer);
  pool.addActor(actor);
  pool.arrayBufferActors.set(buffer, actor);
  return actor.grip();
}

module.exports = {
  ArrayBufferActor,
  arrayBufferGrip,
};
