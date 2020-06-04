/*
 * A socket.io encoder and decoder written in JavaScript complying with version 4
 * of socket.io-protocol. Used by socket.io and socket.io-client.
 *
 * Copyright (c) 2014 Guillermo Rauch <guillermo@learnboost.com>
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the library source tree.
 *
 * https://github.com/socketio/socket.io-parser
 */

/* eslint-disable no-undef */

"use strict";

var withNativeBuffer =
  typeof Buffer === "function" && typeof Buffer.isBuffer === "function";
var withNativeArrayBuffer = typeof ArrayBuffer === "function";

var isView = function(obj) {
  return typeof ArrayBuffer.isView === "function"
    ? ArrayBuffer.isView(obj)
    : obj.buffer instanceof ArrayBuffer;
};

/**
 * Returns true if obj is a buffer or an arraybuffer.
 *
 * @api private
 */

function isBuf(obj) {
  return (
    (withNativeBuffer && Buffer.isBuffer(obj)) ||
    (withNativeArrayBuffer && (obj instanceof ArrayBuffer || isView(obj)))
  );
}

module.exports = isBuf;
