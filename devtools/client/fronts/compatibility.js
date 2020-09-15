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

  /*
   * Backwards compatibility wrapper for getCSSDeclarationBlockIssues.
   * This can be removed once FF82 hits the release channel
   */
  async _getTraits() {
    if (!this._traits) {
      try {
        // From FF82+, getTraits() is supported
        const { traits } = await super.getTraits();
        this._traits = traits;
      } catch (e) {
        this._traits = {};
      }
    }

    return this._traits;
  }

  /*
   * Backwards compatibility wrapper for getCSSDeclarationBlockIssues.
   * This can be removed once FF82 hits the release channel
   */
  async getCSSDeclarationBlockIssues(declarationBlock, targetBrowsers) {
    const traits = await this._getTraits();
    if (!traits.declarationBlockIssueComputationEnabled) {
      return [];
    }

    return super.getCSSDeclarationBlockIssues(declarationBlock, targetBrowsers);
  }
}

exports.CompatibilityFront = CompatibilityFront;
registerFront(CompatibilityFront);
