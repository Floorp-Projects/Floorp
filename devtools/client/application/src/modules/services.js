/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ManifestDevToolsError extends Error {
  constructor(...params) {
    super(...params);

    this.name = "ManifestDevToolsError";
  }
}

class Services {
  init(toolbox) {
    this._toolbox = toolbox;
  }

  selectTool(toolId) {
    this._assertInit();
    return this._toolbox.selectTool(toolId);
  }

  async fetchManifest() {
    let response;

    try {
      this._assertInit();
      const manifestFront = await this._toolbox.target.getFront("manifest");
      response = await manifestFront.fetchCanonicalManifest();
    } catch (error) {
      throw new ManifestDevToolsError(
        error.message,
        error.fileName,
        error.lineNumber
      );
    }

    if (response.errorMessage) {
      throw new Error(response.errorMessage);
    }

    return response.manifest;
  }

  _assertInit() {
    if (!this._toolbox) {
      throw new Error("Services singleton has not been initialized");
    }
  }
}

module.exports = {
  ManifestDevToolsError,
  // exports a singleton, which will be used across all application panel modules
  services: new Services(),
};
