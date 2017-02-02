/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { functionCallSpec, callWatcherSpec } = require("devtools/shared/specs/call-watcher");
const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the FunctionCallActor.
 */
const FunctionCallFront = protocol.FrontClassWithSpec(functionCallSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  /**
   * Adds some generic information directly to this instance,
   * to avoid extra roundtrips.
   */
  form: function (form) {
    this.actorID = form.actor;
    this.type = form.type;
    this.name = form.name;
    this.file = form.file;
    this.line = form.line;
    this.timestamp = form.timestamp;
    this.callerPreview = form.callerPreview;
    this.argsPreview = form.argsPreview;
    this.resultPreview = form.resultPreview;
  }
});

exports.FunctionCallFront = FunctionCallFront;

/**
 * The corresponding Front object for the CallWatcherActor.
 */
var CallWatcherFront =
exports.CallWatcherFront =
protocol.FrontClassWithSpec(callWatcherSpec, {
  initialize: function (client, { callWatcherActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: callWatcherActor });
    this.manage(this);
  }
});

/**
 * Constants.
 */
CallWatcherFront.METHOD_FUNCTION = 0;
CallWatcherFront.GETTER_FUNCTION = 1;
CallWatcherFront.SETTER_FUNCTION = 2;

CallWatcherFront.KNOWN_METHODS = {};

CallWatcherFront.KNOWN_METHODS.CanvasRenderingContext2D = {
  drawWindow: {
    enums: new Set([6])
  },
};

CallWatcherFront.KNOWN_METHODS.WebGLRenderingContext = {
  activeTexture: {
    enums: new Set([0]),
  },
  bindBuffer: {
    enums: new Set([0]),
  },
  bindFramebuffer: {
    enums: new Set([0]),
  },
  bindRenderbuffer: {
    enums: new Set([0]),
  },
  bindTexture: {
    enums: new Set([0]),
  },
  blendEquation: {
    enums: new Set([0]),
  },
  blendEquationSeparate: {
    enums: new Set([0, 1]),
  },
  blendFunc: {
    enums: new Set([0, 1]),
  },
  blendFuncSeparate: {
    enums: new Set([0, 1, 2, 3]),
  },
  bufferData: {
    enums: new Set([0, 1, 2]),
  },
  bufferSubData: {
    enums: new Set([0, 1]),
  },
  checkFramebufferStatus: {
    enums: new Set([0]),
  },
  clear: {
    enums: new Set([0]),
  },
  compressedTexImage2D: {
    enums: new Set([0, 2]),
  },
  compressedTexSubImage2D: {
    enums: new Set([0, 6]),
  },
  copyTexImage2D: {
    enums: new Set([0, 2]),
  },
  copyTexSubImage2D: {
    enums: new Set([0]),
  },
  createShader: {
    enums: new Set([0]),
  },
  cullFace: {
    enums: new Set([0]),
  },
  depthFunc: {
    enums: new Set([0]),
  },
  disable: {
    enums: new Set([0]),
  },
  drawArrays: {
    enums: new Set([0]),
  },
  drawElements: {
    enums: new Set([0, 2]),
  },
  enable: {
    enums: new Set([0]),
  },
  framebufferRenderbuffer: {
    enums: new Set([0, 1, 2]),
  },
  framebufferTexture2D: {
    enums: new Set([0, 1, 2]),
  },
  frontFace: {
    enums: new Set([0]),
  },
  generateMipmap: {
    enums: new Set([0]),
  },
  getBufferParameter: {
    enums: new Set([0, 1]),
  },
  getParameter: {
    enums: new Set([0]),
  },
  getFramebufferAttachmentParameter: {
    enums: new Set([0, 1, 2]),
  },
  getProgramParameter: {
    enums: new Set([1]),
  },
  getRenderbufferParameter: {
    enums: new Set([0, 1]),
  },
  getShaderParameter: {
    enums: new Set([1]),
  },
  getShaderPrecisionFormat: {
    enums: new Set([0, 1]),
  },
  getTexParameter: {
    enums: new Set([0, 1]),
  },
  getVertexAttrib: {
    enums: new Set([1]),
  },
  getVertexAttribOffset: {
    enums: new Set([1]),
  },
  hint: {
    enums: new Set([0, 1]),
  },
  isEnabled: {
    enums: new Set([0]),
  },
  pixelStorei: {
    enums: new Set([0]),
  },
  readPixels: {
    enums: new Set([4, 5]),
  },
  renderbufferStorage: {
    enums: new Set([0, 1]),
  },
  stencilFunc: {
    enums: new Set([0]),
  },
  stencilFuncSeparate: {
    enums: new Set([0, 1]),
  },
  stencilMaskSeparate: {
    enums: new Set([0]),
  },
  stencilOp: {
    enums: new Set([0, 1, 2]),
  },
  stencilOpSeparate: {
    enums: new Set([0, 1, 2, 3]),
  },
  texImage2D: {
    enums: args => args.length > 6 ? new Set([0, 2, 6, 7]) : new Set([0, 2, 3, 4]),
  },
  texParameterf: {
    enums: new Set([0, 1]),
  },
  texParameteri: {
    enums: new Set([0, 1, 2]),
  },
  texSubImage2D: {
    enums: args => args.length === 9 ? new Set([0, 6, 7]) : new Set([0, 4, 5]),
  },
  vertexAttribPointer: {
    enums: new Set([2])
  },
};
