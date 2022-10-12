/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("resource://devtools/shared/protocol.js");
const { Arg, RetVal, generateActorSpec } = protocol;

const longStringSpec = generateActorSpec({
  typeName: "longstractor",

  methods: {
    substring: {
      request: {
        start: Arg(0),
        end: Arg(1),
      },
      response: { substring: RetVal() },
    },
    release: { release: true },
  },
});

exports.longStringSpec = longStringSpec;

/**
 * When a caller is expecting a LongString actor but the string is already available on
 * client, the SimpleStringFront can be used as it shares the same API as a
 * LongStringFront but will not make unnecessary trips to the server.
 */
class SimpleStringFront {
  constructor(str) {
    this.str = str;
  }

  get length() {
    return this.str.length;
  }

  get initial() {
    return this.str;
  }

  string() {
    return Promise.resolve(this.str);
  }

  substring(start, end) {
    return Promise.resolve(this.str.substring(start, end));
  }

  release() {
    this.str = null;
    return Promise.resolve(undefined);
  }
}

exports.SimpleStringFront = SimpleStringFront;

// The long string actor needs some custom marshalling, because it is sometimes
// returned as a primitive rather than a complete form.

var stringActorType = protocol.types.getType("longstractor");
protocol.types.addType("longstring", {
  _actor: true,
  write: (value, context, detail) => {
    if (!(context instanceof protocol.Actor)) {
      throw Error("Passing a longstring as an argument isn't supported.");
    }

    if (value.short) {
      return value.str;
    }
    return stringActorType.write(value, context, detail);
  },
  read: (value, context, detail) => {
    if (context instanceof protocol.Actor) {
      throw Error("Passing a longstring as an argument isn't supported.");
    }
    if (typeof value === "string") {
      return new SimpleStringFront(value);
    }
    return stringActorType.read(value, context, detail);
  },
});
