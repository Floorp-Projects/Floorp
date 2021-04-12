/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { Cu } = require("chrome");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");

function MemoryPanel(iframeWindow, toolbox, commands) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._commands = commands;

  const { BrowserLoader } = Cu.import(
    "resource://devtools/client/shared/browser-loader.js"
  );
  const browserRequire = BrowserLoader({
    baseURI: "resource://devtools/client/memory/",
    window: this.panelWin,
  }).require;
  this.initializer = browserRequire("devtools/client/memory/initializer");

  this._onTargetAvailable = this._onTargetAvailable.bind(this);

  EventEmitter.decorate(this);
}

MemoryPanel.prototype = {
  async open() {
    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gHeapAnalysesClient = new HeapAnalysesClient();

    await this.initializer.initialize();

    await this._commands.targetCommand.watchTargets(
      [this._commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable
    );

    return this;
  },

  async _onTargetAvailable({ targetFront }) {
    if (targetFront.isTopLevel) {
      const front = await targetFront.getFront("memory");
      await front.attach();
      this.initializer.updateFront(front);
    }
  },

  // DevToolPanel API

  destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this._commands.targetCommand.unwatchTargets(
      [this._commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable
    );

    this.initializer.destroy();

    this.panelWin.gHeapAnalysesClient.destroy();
    this.panelWin = null;
    this.emit("destroyed");
  },
};

exports.MemoryPanel = MemoryPanel;
