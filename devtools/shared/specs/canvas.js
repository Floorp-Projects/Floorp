/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const {Arg, Option, RetVal, generateActorSpec} = protocol;

/**
 * Type representing an ArrayBufferView, serialized fast(er).
 *
 * Don't create a new array buffer view from the parsed array on the frontend.
 * Consumers may copy the data into an existing buffer, or create a new one if
 * necesasry. For example, this avoids the need for a redundant copy when
 * populating ImageData objects, at the expense of transferring char views
 * of a pixel buffer over the protocol instead of a packed int view.
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the buffer directly to the front, instead of going through all this
 * serialization redundancy.
 */
protocol.types.addType("array-buffer-view", {
  write: (v) => "[" + Array.join(v, ",") + "]",
  read: (v) => JSON.parse(v)
});

/**
 * Type describing a thumbnail or screenshot in a recorded animation frame.
 */
protocol.types.addDictType("snapshot-image", {
  index: "number",
  width: "number",
  height: "number",
  scaling: "number",
  flipped: "boolean",
  pixels: "array-buffer-view"
});

/**
 * Type describing an overview of a recorded animation frame.
 */
protocol.types.addDictType("snapshot-overview", {
  calls: "array:function-call",
  thumbnails: "array:snapshot-image",
  screenshot: "snapshot-image"
});

exports.CANVAS_CONTEXTS = [
  "CanvasRenderingContext2D",
  "WebGLRenderingContext"
];

exports.ANIMATION_GENERATORS = [
  "requestAnimationFrame"
];

exports.LOOP_GENERATORS = [
  "setTimeout"
];

exports.DRAW_CALLS = [
  // 2D canvas
  "fill",
  "stroke",
  "clearRect",
  "fillRect",
  "strokeRect",
  "fillText",
  "strokeText",
  "drawImage",

  // WebGL
  "clear",
  "drawArrays",
  "drawElements",
  "finish",
  "flush"
];

exports.INTERESTING_CALLS = [
  // 2D canvas
  "save",
  "restore",

  // WebGL
  "useProgram"
];

const frameSnapshotSpec = generateActorSpec({
  typeName: "frame-snapshot",

  methods: {
    getOverview: {
      response: { overview: RetVal("snapshot-overview") }
    },
    generateScreenshotFor: {
      request: { call: Arg(0, "function-call") },
      response: { screenshot: RetVal("snapshot-image") }
    },
  },
});

exports.frameSnapshotSpec = frameSnapshotSpec;

const canvasSpec = generateActorSpec({
  typeName: "canvas",

  methods: {
    setup: {
      request: { reload: Option(0, "boolean") },
      oneway: true
    },
    finalize: {
      oneway: true
    },
    isInitialized: {
      response: { initialized: RetVal("boolean") }
    },
    isRecording: {
      response: { recording: RetVal("boolean") }
    },
    recordAnimationFrame: {
      response: { snapshot: RetVal("nullable:frame-snapshot") }
    },
    stopRecordingAnimationFrame: {
      oneway: true
    },
  }
});

exports.canvasSpec = canvasSpec;
