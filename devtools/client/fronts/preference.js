/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  preferenceSpec,
} = require("resource://devtools/shared/specs/preference.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class PreferenceFront extends FrontClassWithSpec(preferenceSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "preferenceActor";
  }

  async getTraits() {
    if (!this._traits) {
      this._traits = await super.getTraits();
    }

    return this._traits;
  }
}

exports.PreferenceFront = PreferenceFront;
registerFront(PreferenceFront);
