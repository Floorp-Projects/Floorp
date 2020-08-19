/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { compatibilitySpec } = require("devtools/shared/specs/compatibility");

const PREF_TARGET_BROWSERS = "devtools.inspector.compatibility.target-browsers";

class CompatibilityFront extends FrontClassWithSpec(compatibilitySpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.client = client;
  }

  async initialize() {
    // Backward-compatibility: can be removed when FF81 is on the release
    // channel. See 1659866.
    const preferenceFront = await this.client.mainRoot.getFront("preference");
    try {
      await preferenceFront.getCharPref(PREF_TARGET_BROWSERS);
    } catch (e) {
      // If `getCharPref` failed, the preference is not set on the server (most
      // likely Fenix). Set a default value, otherwise compatibility actor
      // methods might throw.
      await preferenceFront.setCharPref(PREF_TARGET_BROWSERS, "");
    }
  }

  // Update the object given a form representation off the wire.
  form(json) {
    this.actorID = json.actor;
  }
}

exports.CompatibilityFront = CompatibilityFront;
registerFront(CompatibilityFront);
