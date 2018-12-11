/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");
const { emulationSpec } = require("devtools/shared/specs/emulation");

/**
 * The corresponding Front object for the EmulationActor.
 */
class EmulationFront extends FrontClassWithSpec(emulationSpec) {
  constructor(client, form) {
    super(client);
    this.actorID = form.emulationActor;
    this.manage(this);
  }

  destroy() {
    super.destroy();
  }
}

exports.EmulationFront = EmulationFront;
registerFront(EmulationFront);
