/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
}

DebuggerPanel.prototype = {
  open: function() {
    let targetPromise;
    if (!this.toolbox.target.isRemote) {
      targetPromise = this.toolbox.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.toolbox.target);
    }

    return targetPromise.then(() => {
      const dbg = this.panelWin.Debugger;
      dbg.setThreadClient(this.toolbox.threadClient);
      dbg.setTabTarget(this.toolbox.target);
      dbg.initPage(dbg.getActions());
      dbg.renderApp();
      return this;
    });
  },

  destroy: function() {
  }
};

exports.DebuggerPanel = DebuggerPanel;
