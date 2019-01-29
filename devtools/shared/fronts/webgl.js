/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  shaderSpec,
  programSpec,
  webGLSpec,
} = require("devtools/shared/specs/webgl");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the ShaderActor.
 */
class ShaderFront extends FrontClassWithSpec(shaderSpec) {
}

exports.ShaderFront = ShaderFront;
registerFront(ShaderFront);

/**
 * The corresponding Front object for the ProgramActor.
 */
class ProgramFront extends FrontClassWithSpec(programSpec) {
}

exports.ProgramFront = ProgramFront;
registerFront(ProgramFront);

/**
 * The corresponding Front object for the WebGLActor.
 */
class WebGLFront extends FrontClassWithSpec(webGLSpec) {
  constructor(client) {
    super(client);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "webglActor";
  }
}

exports.WebGLFront = WebGLFront;
registerFront(WebGLFront);
