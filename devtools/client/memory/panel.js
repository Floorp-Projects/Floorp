/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { MemoryFront } = require("devtools/shared/fronts/memory");
const { Cu } = require("chrome");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");

function MemoryPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  const { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
  const browserRequire = BrowserLoader({
    baseURI: "resource://devtools/client/memory/",
    window: this.panelWin
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

    const rootForm = await this.target.root;
    this.panelWin.gFront = new MemoryFront(this.target.client,
                                           this.target.form,
                                           rootForm);
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

  async destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    await this.panelWin.gFront.detach();

    this._destroyer = this.initializer.destroy().then(() => {
      // Destroy front to ensure packet handler is removed from client
      this.panelWin.gFront.destroy();
      this.panelWin.gHeapAnalysesClient.destroy();
      this.panelWin = null;
      this._opening = null;
      this.isReady = false;
      this.emit("destroyed");
    });

    return this._destroyer;
  }
};

exports.MemoryPanel = MemoryPanel;
