/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol.js");

types.addDictType("imageData", {
  // The image data
  data: "nullable:longstring",
  // The original image dimensions
  size: "json",
});

types.addDictType("windowDimensions", {
  // The window innerWidth
  innerWidth: "nullable:number",
  // The window innerHeight
  innerHeight: "nullable:number",
});

/**
 * Returned from any call that might return a node that isn't connected to root
 * by nodes the child has seen, such as querySelector.
 */
types.addDictType("disconnectedNode", {
  // The actual node to return
  node: "domnode",

  // Nodes that are needed to connect the node to a node the client has already
  // seen
  newParents: "array:domnode",
});

types.addDictType("disconnectedNodeArray", {
  // The actual node list to return
  nodes: "array:domnode",

  // Nodes that are needed to connect those nodes to the root.
  newParents: "array:domnode",
});

const nodeListSpec = generateActorSpec({
  typeName: "domnodelist",

  methods: {
    item: {
      request: { item: Arg(0) },
      response: RetVal("disconnectedNode"),
    },
    items: {
      request: {
        start: Arg(0, "nullable:number"),
        end: Arg(1, "nullable:number"),
      },
      response: RetVal("disconnectedNodeArray"),
    },
    release: {
      release: true,
    },
  },
});

exports.nodeListSpec = nodeListSpec;

const nodeSpec = generateActorSpec({
  typeName: "domnode",

  methods: {
    getNodeValue: {
      request: {},
      response: {
        value: RetVal("longstring"),
      },
    },
    setNodeValue: {
      request: { value: Arg(0) },
      response: {},
    },
    getUniqueSelector: {
      request: {},
      response: {
        value: RetVal("string"),
      },
    },
    getCssPath: {
      request: {},
      response: {
        value: RetVal("string"),
      },
    },
    getXPath: {
      request: {},
      response: {
        value: RetVal("string"),
      },
    },
    scrollIntoView: {
      request: {},
      response: {},
    },
    getImageData: {
      request: { maxDim: Arg(0, "nullable:number") },
      response: RetVal("imageData"),
    },
    getEventListenerInfo: {
      request: {},
      response: {
        events: RetVal("json"),
      },
    },
    modifyAttributes: {
      request: {
        modifications: Arg(0, "array:json"),
      },
      response: {},
    },
    getFontFamilyDataURL: {
      request: { font: Arg(0, "string"), fillStyle: Arg(1, "nullable:string") },
      response: RetVal("imageData"),
    },
    getClosestBackgroundColor: {
      request: {},
      response: {
        value: RetVal("string"),
      },
    },
    getBackgroundColor: {
      request: {},
      response: RetVal("json"),
    },
    getOwnerGlobalDimensions: {
      request: {},
      response: RetVal("windowDimensions"),
    },
    connectToRemoteFrame: {
      request: {},
      // We are passing a target actor form here.
      // As we are manually fetching the form JSON via frame-connector.js connectToFrame,
      // we are not instanciating a protocol.js front class and can't use proper type
      // here and have automatic marshalling.
      //
      // Alex: Can we do something to address that??
      response: RetVal("json"),
    },
  },
});

exports.nodeSpec = nodeSpec;
