/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class Services {
  init(toolbox) {
    this._toolbox = toolbox;
  }

  selectTool(toolId) {
    if (!this._toolbox) {
      throw new Error("Services singleton has not been initialized");
    }
    return this._toolbox.selectTool(toolId);
  }
}

// exports a singleton, which will be used across all application panel modules
exports.services = new Services();
