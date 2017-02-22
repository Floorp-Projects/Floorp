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
    if (!this.toolbox.target.isRemote) {
      await this.toolbox.target.makeRemote();
    }
    await this.panelWin.Netmonitor.bootstrap({
      tabTarget: this.toolbox.target,
      toolbox: this.toolbox,
    });
    this.emit("ready");
    this.isReady = true;
    return this;
  },

  async destroy() {
    await this.panelWin.Netmonitor.destroy();
    this.emit("destroyed");
    return this;
  },
};

exports.NetMonitorPanel = NetMonitorPanel;
