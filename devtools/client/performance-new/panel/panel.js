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
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 * @typedef {import("../@types/perf").Toolbox} Toolbox
 * @typedef {import("../@types/perf").Target} Target
 * @typedef {import("../@types/perf").Commands} Commands
 */

class PerformancePanel {
  /**
   * @param {PanelWindow} iframeWindow
   * @param {Toolbox} toolbox
   * @param {Commands} commands
   */
  constructor(iframeWindow, toolbox, commands) {
    this.panelWin = iframeWindow;
    this.toolbox = toolbox;
    this.commands = commands;

    const EventEmitter = require("resource://devtools/shared/event-emitter.js");
    EventEmitter.decorate(this);
  }

  /**
   * This is implemented (and overwritten) by the EventEmitter. Is there a way
   * to use mixins with JSDoc?
   *
   * @param {string} _eventName
   */
  emit(_eventName) {}

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
    this.panelWin.gIsPanelDestroyed = false;

    const perfFront = await this.commands.client.mainRoot.getFront("perf");

    // Note: we are not using traits in the panel at the moment but we keep the
    // wiring in case we need it later on.
    const traits = {};

    await this.panelWin.gInit(
      perfFront,
      traits,
      "devtools",
      this._openAboutProfiling
    );
    return this;
  }

  _openAboutProfiling() {
    const {
      openTrustedLink,
    } = require("resource://devtools/client/shared/link.js");
    openTrustedLink("about:profiling", {});
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
    this.panelWin.gIsPanelDestroyed = true;
  }
}

exports.PerformancePanel = PerformancePanel;
