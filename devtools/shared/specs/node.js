/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol.js");

types.addDictType("imageData", {
  // The image data
  data: "nullable:longstring",
  // The original image dimensions
  size: "json"
});

const nodeSpec = generateActorSpec({
  typeName: "domnode",

  methods: {
    getNodeValue: {
      request: {},
      response: {
        value: RetVal("longstring")
      }
    },
    setNodeValue: {
      request: { value: Arg(0) },
      response: {}
    },
    getUniqueSelector: {
      request: {},
      response: {
        value: RetVal("string")
      }
    },
    scrollIntoView: {
      request: {},
      response: {}
    },
    getImageData: {
      request: {maxDim: Arg(0, "nullable:number")},
      response: RetVal("imageData")
    },
    getEventListenerInfo: {
      request: {},
      response: {
        events: RetVal("json")
      }
    },
    modifyAttributes: {
      request: {
        modifications: Arg(0, "array:json")
      },
      response: {}
    },
    getFontFamilyDataURL: {
      request: {font: Arg(0, "string"), fillStyle: Arg(1, "nullable:string")},
      response: RetVal("imageData")
    }
  }
});

exports.nodeSpec = nodeSpec;
