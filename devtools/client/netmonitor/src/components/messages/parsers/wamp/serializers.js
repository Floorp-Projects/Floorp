/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  parseWampArray,
} = require("resource://devtools/client/netmonitor/src/components/messages/parsers/wamp/arrayParser.js");
const msgpack = require("resource://devtools/client/netmonitor/src/components/messages/msgpack.js");
const cbor = require("resource://devtools/client/netmonitor/src/components/messages/cbor.js");

class WampSerializer {
  deserializeMessage(payload) {
    const array = this.deserializeToArray(payload);
    const result = parseWampArray(array);
    return result;
  }

  stringToBinary(str) {
    const result = new Uint8Array(str.length);
    for (let i = 0; i < str.length; i++) {
      result[i] = str[i].charCodeAt(0);
    }
    return result;
  }
}

class JsonSerializer extends WampSerializer {
  constructor() {
    super(...arguments);
    this.subProtocol = "wamp.2.json";
    this.description = "WAMP JSON";
  }
  deserializeToArray(payload) {
    return JSON.parse(payload);
  }
}

class MessagePackSerializer extends WampSerializer {
  constructor() {
    super(...arguments);
    this.subProtocol = "wamp.2.msgpack";
    this.description = "WAMP MessagePack";
  }
  deserializeToArray(payload) {
    const binary = this.stringToBinary(payload);
    return msgpack.deserialize(binary);
  }
}

class CBORSerializer extends WampSerializer {
  constructor() {
    super(...arguments);
    this.subProtocol = "wamp.2.cbor";
    this.description = "WAMP CBOR";
  }
  deserializeToArray(payload) {
    const binaryBuffer = this.stringToBinary(payload).buffer;
    return cbor.decode(binaryBuffer);
  }
}

const serializers = {};
for (var serializer of [
  new JsonSerializer(),
  new MessagePackSerializer(),
  new CBORSerializer(),
]) {
  serializers[serializer.subProtocol] = serializer;
}

module.exports = { wampSerializers: serializers };
