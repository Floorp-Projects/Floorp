/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function NetMonitorPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.panelDoc = iframeWindow.document;
  this.toolbox = toolbox;
  this.netmonitor = new iframeWindow.Netmonitor(toolbox);
}

NetMonitorPanel.prototype = {
  open: async function () {
    if (!this.toolbox.target.isRemote) {
      await this.toolbox.target.makeRemote();
    }
    await this.netmonitor.init();
    this.emit("ready");
    this.isReady = true;
    return this;
  },

  destroy: async function () {
    await this.netmonitor.destroy();
    this.emit("destroyed");
    return this;
  },
};

exports.NetMonitorPanel = NetMonitorPanel;
