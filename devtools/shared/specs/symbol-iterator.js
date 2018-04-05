/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Option,
  RetVal,
  types,
} = require("devtools/shared/protocol");

types.addDictType("symboliterator.data", {
  ownSymbols: "array:symboliterator.ownsymbols",
});

types.addDictType("symboliterator.ownsymbols", {
  name: "string",
  descriptor: "nullable:object.descriptor",
});

// XXX Move to the object spec in Bug 1450944.
types.addDictType("object.descriptor", {
  configurable: "boolean",
  enumerable: "boolean",
  // Can be null if there is a getter for the property.
  value: "nullable:json",
  // Only set `value` exists.
  writable: "nullable:boolean",
  // Only set when `value` does not exist and there is a getter for the property.
  get: "nullable:json",
  // Only set when `value` does not exist and there is a setter for the property.
  set: "nullable:json",
});

const symbolIteratorSpec = generateActorSpec({
  typeName: "symbolIterator",

  methods: {
    slice: {
      request: {
        start: Option(0, "number"),
        count: Option(0, "number"),
      },
      response: RetVal("symboliterator.data")
    },
    all: {
      request: {},
      response: RetVal("symboliterator.data")
    },
  }
});

exports.symbolIteratorSpec = symbolIteratorSpec;
