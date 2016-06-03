/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  shaderSpec,
  programSpec,
  webGLSpec,
} = require("devtools/shared/specs/webgl");
const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the ShaderActor.
 */
const ShaderFront = protocol.FrontClassWithSpec(shaderSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.ShaderFront = ShaderFront;

/**
 * The corresponding Front object for the ProgramActor.
 */
const ProgramFront = protocol.FrontClassWithSpec(programSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.ProgramFront = ProgramFront;

/**
 * The corresponding Front object for the WebGLActor.
 */
const WebGLFront = protocol.FrontClassWithSpec(webGLSpec, {
  initialize: function (client, { webglActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: webglActor });
    this.manage(this);
  }
});

exports.WebGLFront = WebGLFront;
