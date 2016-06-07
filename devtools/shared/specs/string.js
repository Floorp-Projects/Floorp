/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const {Arg, RetVal, generateActorSpec} = protocol;

loader.lazyRequireGetter(this, "SimpleStringFront",
                         "devtools/shared/fronts/string", true);

const longStringSpec = generateActorSpec({
  typeName: "longstractor",

  methods: {
    substring: {
      request: {
        start: Arg(0),
        end: Arg(1)
      },
      response: { substring: RetVal() },
    },
    release: { release: true },
  },
});

exports.longStringSpec = longStringSpec;

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
    if (typeof (value) === "string") {
      return SimpleStringFront(value);
    }
    return stringActorType.read(value, context, detail);
  }
});
