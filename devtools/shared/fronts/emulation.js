/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClass } = require("devtools/shared/protocol");
const { emulationSpec } = require("devtools/shared/specs/emulation");

/**
 * The corresponding Front object for the EmulationActor.
 */
const EmulationFront = FrontClass(emulationSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client);
    this.actorID = form.emulationActor;
    this.manage(this);
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },
});

exports.EmulationFront = EmulationFront;
