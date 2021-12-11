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

"use strict";

/**
 * Reconstructs a binary packet from its placeholder packet and buffers
 *
 * @param {Object} packet - event packet with placeholders
 * @param {Array} buffers - binary buffers to put in placeholder positions
 * @return {Object} reconstructed packet
 * @api public
 */

exports.reconstructPacket = function(packet, buffers) {
  packet.data = _reconstructPacket(packet.data, buffers);
  packet.attachments = undefined; // no longer useful
  return packet;
};

function _reconstructPacket(data, buffers) {
  if (!data) {
    return data;
  }

  if (data && data._placeholder) {
    return buffers[data.num]; // appropriate buffer (should be natural order anyway)
  } else if (Array.isArray(data)) {
    for (let i = 0; i < data.length; i++) {
      data[i] = _reconstructPacket(data[i], buffers);
    }
  } else if (typeof data === "object") {
    for (const key in data) {
      data[key] = _reconstructPacket(data[key], buffers);
    }
  }

  return data;
}
