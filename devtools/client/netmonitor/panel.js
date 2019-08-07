/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function NetMonitorPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
}

NetMonitorPanel.prototype = {
  async open() {
    // Reuse an existing Network monitor API object if available.
    // It could have been created for WE API before Net panel opens.
    const api = await this.toolbox.getNetMonitorAPI();
    const app = this.panelWin.initialize(api);

    // Connect the application object to the UI.
    await app.bootstrap({
      toolbox: this.toolbox,
      document: this.panelWin.document,
      win: this.panelWin,
    });

    // Ready to go!
    this.emit("ready");
    this.isReady = true;

    return this;
  },

  destroy() {
    this.panelWin.Netmonitor.destroy();
    this.emit("destroyed");
  },
};

exports.NetMonitorPanel = NetMonitorPanel;
