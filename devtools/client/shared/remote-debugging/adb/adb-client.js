/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A module to track device changes
 * Adapted from adb.js at
 * https://github.com/mozilla/adbhelper/tree/f44386c2d8cb7635a7d2c5a51191c89b886f8327
 */

"use strict";

const {
  AdbSocket,
} = require("devtools/client/shared/remote-debugging/adb/adb-socket");
const { dumpn } = require("devtools/shared/DevToolsUtils");

const OKAY = 0x59414b4f;
const FAIL = 0x4c494146;

// Return buffer, which differs between Gecko versions
function getBuffer(packet) {
  return packet.buffer ? packet.buffer : packet;
}

// @param aPacket         The packet to get the length from.
// @param aIgnoreResponse True if this packet has no OKAY/FAIL.
// @return                A js object { length:...; data:... }
function unpackPacket(packet, ignoreResponse) {
  const buffer = getBuffer(packet);
  dumpn("Len buffer: " + buffer.byteLength);
  if (buffer.byteLength === 4 && !ignoreResponse) {
    dumpn("Packet empty");
    return { length: 0, data: "" };
  }
  const lengthView = new Uint8Array(buffer, ignoreResponse ? 0 : 4, 4);
  const decoder = new TextDecoder();
  const length = parseInt(decoder.decode(lengthView), 16);
  const text = new Uint8Array(buffer, ignoreResponse ? 4 : 8, length);
  return { length, data: decoder.decode(text) };
}

// Checks if the response is expected (defaults to OKAY).
// @return true if response equals expected.
function checkResponse(packet, expected = OKAY) {
  const buffer = getBuffer(packet);
  const view = new Uint32Array(buffer, 0, 1);
  if (view[0] == FAIL) {
    dumpn("Response: FAIL");
  }
  dumpn("view[0] = " + view[0]);
  return view[0] == expected;
}

// @param aCommand A protocol-level command as described in
//  http://androidxref.com/4.0.4/xref/system/core/adb/OVERVIEW.TXT and
//  http://androidxref.com/4.0.4/xref/system/core/adb/SERVICES.TXT
// @return A 8 bit typed array.
function createRequest(command) {
  let length = command.length.toString(16).toUpperCase();
  while (length.length < 4) {
    length = "0" + length;
  }

  const encoder = new TextEncoder();
  dumpn("Created request: " + length + command);
  return encoder.encode(length + command);
}

function connect() {
  return new AdbSocket();
}

const client = {
  getBuffer,
  unpackPacket,
  checkResponse,
  createRequest,
  connect,
};

module.exports = client;
