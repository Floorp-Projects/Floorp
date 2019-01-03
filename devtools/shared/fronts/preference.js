/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {preferenceSpec} = require("devtools/shared/specs/preference");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

class PreferenceFront extends FrontClassWithSpec(preferenceSpec) {
  constructor(client, form) {
    super(client, { actor: form.preferenceActor });
    this.manage(this);
  }
}

exports.PreferenceFront = PreferenceFront;
registerFront(PreferenceFront);
