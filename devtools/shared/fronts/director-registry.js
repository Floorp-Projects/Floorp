/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {directorRegistrySpec} = require("devtools/shared/specs/director-registry");
const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the DirectorRegistryActor.
 */
const DirectorRegistryFront = protocol.FrontClassWithSpec(directorRegistrySpec, {
  initialize: function (client, { directorRegistryActor }) {
    protocol.Front.prototype.initialize.call(this, client, {
      actor: directorRegistryActor
    });
    this.manage(this);
  }
});

exports.DirectorRegistryFront = DirectorRegistryFront;
