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

/* eslint-disable no-unused-vars */

"use strict";

const Emitter = require("devtools/client/netmonitor/src/components/websockets/parsers/socket-io/component-emitter.js");
const binary = require("devtools/client/netmonitor/src/components/websockets/parsers/socket-io/binary.js");
const isBuf = require("devtools/client/netmonitor/src/components/websockets/parsers/socket-io/is-buffer.js");

/**
 * Packet types
 */

const TYPES = [
  "CONNECT",
  "DISCONNECT",
  "EVENT",
  "ACK",
  "ERROR",
  "BINARY_EVENT",
  "BINARY_ACK",
];

/**
 * Packet type `connect`
 */

const CONNECT = 0;

/**
 * Packet type `disconnect`
 */

const DISCONNECT = 1;

/**
 * Packet type `event`
 */

const EVENT = 2;

/**
 * Packet type `ack`
 */

const ACK = 3;

/**
 * Packet type `error`
 */

const ERROR = 4;

/**
 * Packet type 'binary event'
 */
const BINARY_EVENT = 5;

/**
 * Packet type `binary ack`. For acks with binary arguments
 */

const BINARY_ACK = 6;

/**
 * A socket.io Decoder instance
 *
 * @return {Object} decoder
 * @api public
 */

function Decoder() {
  this.reconstructor = null;
}

/**
 * Mix in `Emitter` with Decoder.
 */

Emitter(Decoder.prototype);

/**
 * A manager of a binary event's 'buffer sequence'. Should
 * be constructed whenever a packet of type BINARY_EVENT is
 * decoded.
 *
 * @param {Object} packet
 * @return {BinaryReconstructor} initialized reconstructor
 * @api private
 */

function BinaryReconstructor(packet) {
  this.reconPack = packet;
  this.buffers = [];
}

/**
 * Method to be called when binary data received from connection
 * after a BINARY_EVENT packet.
 *
 * @param {Buffer | ArrayBuffer} binData - the raw binary data received
 * @return {null | Object} returns null if more binary data is expected or
 *   a reconstructed packet object if all buffers have been received.
 * @api private
 */

BinaryReconstructor.prototype.takeBinaryData = function(binData) {
  this.buffers.push(binData);
  if (this.buffers.length === this.reconPack.attachments) {
    // done with buffer list
    const packet = binary.reconstructPacket(this.reconPack, this.buffers);
    this.finishedReconstruction();
    return packet;
  }
  return null;
};

/**
 * Cleans up binary packet reconstruction variables.
 *
 * @api private
 */

BinaryReconstructor.prototype.finishedReconstruction = function() {
  this.reconPack = null;
  this.buffers = [];
};

/**
 * Decodes an encoded packet string into packet JSON.
 *
 * @param {String} obj - encoded packet
 * @return {Object} packet
 * @api public
 */

Decoder.prototype.add = function(obj) {
  let packet;
  if (typeof obj === "string") {
    packet = decodeString(obj);
    if (BINARY_EVENT === packet.type || BINARY_ACK === packet.type) {
      // binary packet's json
      this.reconstructor = new BinaryReconstructor(packet);

      // no attachments, labeled binary but no binary data to follow
      if (this.reconstructor.reconPack.attachments === 0) {
        this.emit("decoded", packet);
      }
    } else {
      // non-binary full packet
      this.emit("decoded", packet);
    }
  } else if (isBuf(obj) || obj.base64) {
    // raw binary data
    if (!this.reconstructor) {
      throw new Error("got binary data when not reconstructing a packet");
    } else {
      packet = this.reconstructor.takeBinaryData(obj);
      if (packet) {
        // received final buffer
        this.reconstructor = null;
        this.emit("decoded", packet);
      }
    }
  } else {
    throw new Error("Unknown type: " + obj);
  }
};

/**
 * Decode a packet String (JSON data)
 *
 * @param {String} str
 * @return {Object} packet
 * @api private
 */
// eslint-disable-next-line complexity
function decodeString(str) {
  let i = 0;
  // look up type
  const p = {
    type: Number(str.charAt(0)),
  };

  if (TYPES[p.type] == null) {
    return error("unknown packet type " + p.type);
  }

  // look up attachments if type binary
  if (BINARY_EVENT === p.type || BINARY_ACK === p.type) {
    let buf = "";
    while (str.charAt(++i) !== "-") {
      buf += str.charAt(i);
      if (i === str.length) {
        break;
      }
    }
    if (buf != Number(buf) || str.charAt(i) !== "-") {
      throw new Error("Illegal attachments");
    }
    p.attachments = Number(buf);
  }

  // look up namespace (if any)
  if (str.charAt(i + 1) === "/") {
    p.nsp = "";
    while (++i) {
      const c = str.charAt(i);
      if (c === ",") {
        break;
      }
      p.nsp += c;
      if (i === str.length) {
        break;
      }
    }
  } else {
    p.nsp = "/";
  }

  // look up id
  const next = str.charAt(i + 1);
  if (next !== "" && Number(next) == next) {
    p.id = "";
    while (++i) {
      const c = str.charAt(i);
      if (c == null || Number(c) != c) {
        --i;
        break;
      }
      p.id += str.charAt(i);
      if (i === str.length) {
        break;
      }
    }
    p.id = Number(p.id);
  }

  // look up json data
  if (str.charAt(++i)) {
    const payload = tryParse(str.substr(i));
    const isPayloadValid =
      payload !== false && (p.type === ERROR || Array.isArray(payload));
    if (isPayloadValid) {
      p.data = payload;
    } else {
      return error("invalid payload");
    }
  }

  return p;
}

function tryParse(str) {
  try {
    return JSON.parse(str);
  } catch (e) {
    return false;
  }
}

/**
 * Deallocates a parser's resources
 *
 * @api public
 */

Decoder.prototype.destroy = function() {
  if (this.reconstructor) {
    this.reconstructor.finishedReconstruction();
  }
};

function error(msg) {
  return {
    type: ERROR,
    data: "parser error: " + msg,
  };
}

module.exports = Decoder;
