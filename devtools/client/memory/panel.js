/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { Cu } = require("chrome");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");

function MemoryPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  const { BrowserLoader } = Cu.import(
    "resource://devtools/client/shared/browser-loader.js"
  );
  const browserRequire = BrowserLoader({
    baseURI: "resource://devtools/client/memory/",
    window: this.panelWin,
  }).require;
  this.initializer = browserRequire("devtools/client/memory/initializer");

  EventEmitter.decorate(this);
}

MemoryPanel.prototype = {
  async open() {
    if (this._opening) {
      return this._opening;
    }

    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;
    this.panelWin.gFront = await this.target.getFront("memory");
    this.panelWin.gHeapAnalysesClient = new HeapAnalysesClient();

    await this.panelWin.gFront.attach();

    this._opening = this.initializer.initialize().then(() => {
      this.isReady = true;
      this.emit("ready");
      return this;
    });

    return this._opening;
  },

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this.initializer.destroy();

    // Destroy front to ensure packet handler is removed from client
    this.panelWin.gFront.destroy();
    this.panelWin.gHeapAnalysesClient.destroy();
    this.panelWin = null;
    this._opening = null;
    this.isReady = false;
    this.emit("destroyed");
  },
};

exports.MemoryPanel = MemoryPanel;
