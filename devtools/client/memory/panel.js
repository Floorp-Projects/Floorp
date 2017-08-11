/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const { MemoryFront } = require("devtools/shared/fronts/memory");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");

function MemoryPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  EventEmitter.decorate(this);
}

MemoryPanel.prototype = {
  open: Task.async(function* () {
    if (this._opening) {
      return this._opening;
    }

    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;

    const rootForm = yield this.target.root;
    this.panelWin.gFront = new MemoryFront(this.target.client,
                                           this.target.form,
                                           rootForm);
    this.panelWin.gHeapAnalysesClient = new HeapAnalysesClient();

    yield this.panelWin.gFront.attach();

    this._opening = this.panelWin.initialize().then(() => {
      this.isReady = true;
      this.emit("ready");
      return this;
    });

    return this._opening;
  }),

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: Task.async(function* () {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    yield this.panelWin.gFront.detach();

    this._destroyer = this.panelWin.destroy().then(() => {
      // Destroy front to ensure packet handler is removed from client
      this.panelWin.gFront.destroy();
      this.panelWin.gHeapAnalysesClient.destroy();
      this.panelWin = null;
      this._opening = null;
      this.isReady = false;
      this.emit("destroyed");
    });

    return this._destroyer;
  })
};

exports.MemoryPanel = MemoryPanel;
