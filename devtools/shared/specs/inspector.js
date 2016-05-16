/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");

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

/**
 * Returned from any call that might return a node that isn't connected to root
 * by nodes the child has seen, such as querySelector.
 */
types.addDictType("disconnectedNode", {
  // The actual node to return
  node: "domnode",

  // Nodes that are needed to connect the node to a node the client has already
  // seen
  newParents: "array:domnode"
});

types.addDictType("disconnectedNodeArray", {
  // The actual node list to return
  nodes: "array:domnode",

  // Nodes that are needed to connect those nodes to the root.
  newParents: "array:domnode"
});

types.addDictType("dommutation", {});

types.addDictType("searchresult", {
  list: "domnodelist",
  // Right now there is isn't anything required for metadata,
  // but it's json so it can be extended with extra data.
  metadata: "array:json"
});

const nodeListSpec = generateActorSpec({
  typeName: "domnodelist",

  methods: {
    item: {
      request: { item: Arg(0) },
      response: RetVal("disconnectedNode")
    },
    items: {
      request: {
        start: Arg(0, "nullable:number"),
        end: Arg(1, "nullable:number")
      },
      response: RetVal("disconnectedNodeArray")
    },
    release: {
      release: true
    }
  }
});

exports.nodeListSpec = nodeListSpec;
