/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class Services {
  init(toolbox) {
    this._toolbox = toolbox;
  }

  selectTool(toolId) {
    this._assertInit();
    return this._toolbox.selectTool(toolId);
  }

  async fetchManifest() {
    this._assertInit();

    const manifestFront = await this._toolbox.target.getFront("manifest");
    const response = await manifestFront.fetchCanonicalManifest();

    return response;
  }

  _assertInit() {
    if (!this._toolbox) {
      throw new Error("Services singleton has not been initialized");
    }
  }
}

// exports a singleton, which will be used across all application panel modules
exports.services = new Services();
