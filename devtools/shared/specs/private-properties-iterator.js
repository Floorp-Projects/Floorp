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

types.addDictType("privatepropertiesiterator.data", {
  privateProperties: "array:privatepropertiesiterator.privateProperties",
});

types.addDictType("privatepropertiesiterator.privateProperties", {
  name: "string",
  descriptor: "nullable:object.descriptor",
});

const privatePropertiesIteratorSpec = generateActorSpec({
  typeName: "privatePropertiesIterator",

  methods: {
    slice: {
      request: {
        start: Option(0, "number"),
        count: Option(0, "number"),
      },
      response: RetVal("privatepropertiesiterator.data"),
    },
    all: {
      request: {},
      response: RetVal("privatepropertiesiterator.data"),
    },
    release: { release: true },
  },
});

exports.privatePropertiesIteratorSpec = privatePropertiesIteratorSpec;
