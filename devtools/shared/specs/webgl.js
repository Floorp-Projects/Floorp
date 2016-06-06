/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, Option, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const shaderSpec = generateActorSpec({
  typeName: "gl-shader",

  methods: {
    getText: {
      response: { text: RetVal("string") }
    },
    compile: {
      request: { text: Arg(0, "string") },
      response: { error: RetVal("nullable:json") }
    },
  },
});

exports.shaderSpec = shaderSpec;

const programSpec = generateActorSpec({
  typeName: "gl-program",

  methods: {
    getVertexShader: {
      response: { shader: RetVal("gl-shader") }
    },
    getFragmentShader: {
      response: { shader: RetVal("gl-shader") }
    },
    highlight: {
      request: { tint: Arg(0, "array:number") },
      oneway: true
    },
    unhighlight: {
      oneway: true
    },
    blackbox: {
      oneway: true
    },
    unblackbox: {
      oneway: true
    },
  }
});

exports.programSpec = programSpec;

const webGLSpec = generateActorSpec({
  typeName: "webgl",

  /**
   * Events emitted by this actor. The "program-linked" event is fired every
   * time a WebGL program was linked with its respective two shaders.
   */
  events: {
    "program-linked": {
      type: "programLinked",
      program: Arg(0, "gl-program")
    },
    "global-destroyed": {
      type: "globalDestroyed",
      program: Arg(0, "number")
    },
    "global-created": {
      type: "globalCreated",
      program: Arg(0, "number")
    }
  },

  methods: {
    setup: {
      request: { reload: Option(0, "boolean") },
      oneway: true
    },
    finalize: {
      oneway: true
    },
    getPrograms: {
      response: { programs: RetVal("array:gl-program") }
    },
    waitForFrame: {
      response: { success: RetVal("nullable:json") }
    },
    getPixel: {
      request: {
        selector: Option(0, "string"),
        position: Option(0, "json")
      },
      response: { pixels: RetVal("json") }
    },
    _getAllPrograms: {
      response: { programs: RetVal("array:gl-program") }
    }
  }
});

exports.webGLSpec = webGLSpec;
