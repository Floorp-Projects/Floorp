/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * This file contains the PerformancePanel, which uses a common API for DevTools to
 * start and load everything. This will call `gInit` from the initializer.js file,
 * which does the important initialization for the panel. This code is more concerned
 * with wiring this panel into the rest of DevTools and fetching the Actor's fronts.
 */

/**
 * @typedef {import("./@types/perf").PanelWindow} PanelWindow
 * @typedef {import("./@types/perf").Toolbox} Toolbox
 * @typedef {import("./@types/perf").Target} Target
 */

class PerformancePanel {
  /**
   * @param {PanelWindow} iframeWindow
   * @param {Toolbox} toolbox
   */
  constructor(iframeWindow, toolbox) {
    this.panelWin = iframeWindow;
    this.toolbox = toolbox;

    const EventEmitter = require("devtools/shared/event-emitter");
    EventEmitter.decorate(this);
  }

  /**
   * This is implemented (and overwritten) by the EventEmitter. Is there a way
   * to use mixins with JSDoc?
   *
   * @param {string} eventName
   */
  emit(eventName) {}

  /**
   * Open is effectively an asynchronous constructor.
   * @return {Promise<PerformancePanel>} Resolves when the Perf tool completes
   *     opening.
   */
  open() {
    if (!this._opening) {
      this._opening = this._doOpen();
    }
    return this._opening;
  }

  /**
   * This function is the actual implementation of the open() method.
   * @returns Promise<PerformancePanel>
   */
  async _doOpen() {
    this.panelWin.gToolbox = this.toolbox;
    this.panelWin.gTarget = this.target;

    const [perfFront, preferenceFront] = await Promise.all([
      this.target.client.mainRoot.getFront("perf"),
      this.target.client.mainRoot.getFront("preference"),
    ]);

    this.isReady = true;
    this.emit("ready");
    this.panelWin.gInit(perfFront, preferenceFront);
    return this;
  }

  // DevToolPanel API:

  /**
   * @returns {Target} target
   */
  get target() {
    return this.toolbox.target;
  }

  destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }
    this.panelWin.gDestroy();
    this.emit("destroyed");
    this._destroyed = true;
  }
}

exports.PerformancePanel = PerformancePanel;
