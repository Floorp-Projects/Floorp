/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

function WhatsNewPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
}

WhatsNewPanel.prototype = {
  open: async function() {
    this.panelWin.WhatsNew.bootstrap();
    this.emit("ready");
    this.isReady = true;
    return this;
  },

  destroy: function() {
    this.panelWin.WhatsNew.destroy();
    this.emit("destroyed");
  },
};

exports.WhatsNewPanel = WhatsNewPanel;
