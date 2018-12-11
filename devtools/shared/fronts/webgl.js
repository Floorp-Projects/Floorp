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
  constructor(client, form) {
    super(client, form);
  }
}

exports.ShaderFront = ShaderFront;
registerFront(ShaderFront);

/**
 * The corresponding Front object for the ProgramActor.
 */
class ProgramFront extends FrontClassWithSpec(programSpec) {
  constructor(client, form) {
    super(client, form);
  }
}

exports.ProgramFront = ProgramFront;
registerFront(ProgramFront);

/**
 * The corresponding Front object for the WebGLActor.
 */
class WebGLFront extends FrontClassWithSpec(webGLSpec) {
  constructor(client, { webglActor }) {
    super(client, { actor: webglActor });
    this.manage(this);
  }
}

exports.WebGLFront = WebGLFront;
registerFront(WebGLFront);
