/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  styleSheetsSpec,
} = require("resource://devtools/shared/specs/style-sheets.js");

/**
 * The corresponding Front object for the StyleSheetsActor.
 */
class StyleSheetsFront extends FrontClassWithSpec(styleSheetsSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "styleSheetsActor";
  }

  async getTraits() {
    if (this._traits) {
      return this._traits;
    }
    const { traits } = await super.getTraits();
    this._traits = traits;
    return this._traits;
  }
}

exports.StyleSheetsFront = StyleSheetsFront;
registerFront(StyleSheetsFront);
