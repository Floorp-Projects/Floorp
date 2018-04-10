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

types.addDictType("propertyiterator.data", {
  ownProperties: "nullable:json",
});

const propertyIteratorSpec = generateActorSpec({
  typeName: "propertyIterator",

  methods: {
    names: {
      request: {
        indexes: Option(0, "array:number"),
      },
      response: RetVal("array:string")
    },
    slice: {
      request: {
        start: Option(0, "number"),
        count: Option(0, "number"),
      },
      response: RetVal("propertyiterator.data")
    },
    all: {
      request: {},
      response: RetVal("propertyiterator.data")
    },
  }
});

exports.propertyIteratorSpec = propertyIteratorSpec;
