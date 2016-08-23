/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/shared/protocol");
const { framerateSpec } = require("devtools/shared/specs/framerate");

/**
 * The corresponding Front object for the FramerateActor.
 */
var FramerateFront = exports.FramerateFront = FrontClassWithSpec(framerateSpec, {
  initialize: function (client, { framerateActor }) {
    Front.prototype.initialize.call(this, client, { actor: framerateActor });
    this.manage(this);
  }
});

exports.FramerateFront = FramerateFront;
