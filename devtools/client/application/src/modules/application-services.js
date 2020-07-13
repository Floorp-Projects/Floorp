/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

class ManifestDevToolsError extends Error {
  constructor(...params) {
    super(...params);

    this.name = "ManifestDevToolsError";
  }
}

class ApplicationServices {
  init(toolbox) {
    this._toolbox = toolbox;

    this.features = {
      doesDebuggerSupportWorkers: Services.prefs.getBoolPref(
        "devtools.debugger.features.windowless-service-workers",
        false
      ),
    };
  }

  selectTool(toolId) {
    this._assertInit();
    return this._toolbox.selectTool(toolId, "application");
  }

  async openWorkerInDebugger(workerTargetFront) {
    const debuggerPanel = await this.selectTool("jsdebugger");
    debuggerPanel.selectWorker(workerTargetFront);
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
  services: new ApplicationServices(),
};
