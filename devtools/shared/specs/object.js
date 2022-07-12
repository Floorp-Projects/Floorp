/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  RetVal,
  types,
} = require("devtools/shared/protocol");

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

types.addDictType("object.completion", {
  return: "nullable:json",
  throw: "nullable:json",
});

types.addDictType("object.prototypeproperties", {
  prototype: "object.descriptor",
  ownProperties: "nullable:json",
  ownSymbols: "nullable:array:object.descriptor",
  safeGetterValues: "nullable:json",
});

types.addDictType("object.prototype", {
  prototype: "object.descriptor",
});

types.addDictType("object.property", {
  descriptor: "nullable:object.descriptor",
});

types.addDictType("object.propertyValue", {
  value: "nullable:object.completion",
});

types.addDictType("object.apply", {
  value: "nullable:object.completion",
});

types.addDictType("object.bindings", {
  arguments: "array:json",
  variables: "json",
});

types.addDictType("object.enumProperties.Options", {
  enumEntries: "nullable:boolean",
  ignoreNonIndexedProperties: "nullable:boolean",
  ignoreIndexedProperties: "nullable:boolean",
  query: "nullable:string",
  sort: "nullable:boolean",
});

types.addDictType("object.dependentPromises", {
  promises: "array:object.descriptor",
});

types.addDictType("object.originalSourceLocation", {
  source: "source",
  line: "number",
  column: "number",
  functionDisplayName: "string",
});

types.addDictType("object.promiseState", {
  state: "string",
  value: "nullable:object.descriptor",
  reason: "nullable:object.descriptor",
  creationTimestamp: "number",
  timeToSettle: "nullable:number",
});

types.addDictType("object.proxySlots", {
  proxyTarget: "object.descriptor",
  proxyHandler: "object.descriptor",
});

types.addDictType("object.customFormatterBody", {
  customFormatterBody: "json",
});

const objectSpec = generateActorSpec({
  typeName: "obj",
  methods: {
    allocationStack: {
      request: {},
      response: {
        allocationStack: RetVal("array:object.originalSourceLocation"),
      },
    },
    dependentPromises: {
      request: {},
      response: RetVal("object.dependentPromises"),
    },
    enumEntries: {
      request: {},
      response: {
        iterator: RetVal("propertyIterator"),
      },
    },
    enumProperties: {
      request: {
        options: Arg(0, "nullable:object.enumProperties.Options"),
      },
      response: {
        iterator: RetVal("propertyIterator"),
      },
    },
    enumPrivateProperties: {
      request: {},
      response: {
        iterator: RetVal("privatePropertiesIterator"),
      },
    },
    enumSymbols: {
      request: {},
      response: {
        iterator: RetVal("symbolIterator"),
      },
    },
    fulfillmentStack: {
      request: {},
      response: {
        fulfillmentStack: RetVal("array:object.originalSourceLocation"),
      },
    },
    prototypeAndProperties: {
      request: {},
      response: RetVal("object.prototypeproperties"),
    },
    prototype: {
      request: {},
      response: RetVal("object.prototype"),
    },
    property: {
      request: {
        name: Arg(0, "string"),
      },
      response: RetVal("object.property"),
    },
    propertyValue: {
      request: {
        name: Arg(0, "string"),
        receiverId: Arg(1, "nullable:string"),
      },
      response: RetVal("object.propertyValue"),
    },
    apply: {
      request: {
        context: Arg(0, "nullable:json"),
        arguments: Arg(1, "nullable:array:json"),
      },
      response: RetVal("object.apply"),
    },
    rejectionStack: {
      request: {},
      response: {
        rejectionStack: RetVal("array:object.originalSourceLocation"),
      },
    },
    promiseState: {
      request: {},
      response: RetVal("object.promiseState"),
    },
    proxySlots: {
      request: {},
      response: RetVal("object.proxySlots"),
    },
    customFormatterBody: {
      request: {
        customFormatterIndex: Arg(0, "number"),
      },
      response: RetVal("object.customFormatterBody"),
    },
    addWatchpoint: {
      request: {
        property: Arg(0, "string"),
        label: Arg(1, "string"),
        watchpointType: Arg(2, "string"),
      },
      oneway: true,
    },
    removeWatchpoint: {
      request: {
        property: Arg(0, "string"),
      },
      oneway: true,
    },
    removeWatchpoints: {
      request: {},
      oneway: true,
    },
    release: { release: true },
    // Needed for the PauseScopedObjectActor which extends the ObjectActor.
    threadGrip: {
      request: {},
      response: {},
    },
  },
});

exports.objectSpec = objectSpec;
