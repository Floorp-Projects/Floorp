/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const {changesSpec} = require("devtools/shared/specs/changes");

/**
 * ChangesFront, the front object for the ChangesActor
 */
const ChangesFront = protocol.FrontClassWithSpec(changesSpec, {
  initialize: function(client, {changesActor}) {
    protocol.Front.prototype.initialize.call(this, client, {actor: changesActor});
    this.manage(this);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },
});

exports.ChangesFront = ChangesFront;
