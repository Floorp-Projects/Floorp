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
