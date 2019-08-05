/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

class PerformancePanel {
  constructor(iframeWindow, toolbox) {
    this.panelWin = iframeWindow;
    this.toolbox = toolbox;

    EventEmitter.decorate(this);
  }

  /**
   * Open is effectively an asynchronous constructor.
   * @return {Promise} Resolves when the Perf tool completes opening.
   */
  open() {
    if (!this._opening) {
      this._opening = this._doOpen();
    }
    return this._opening;
  }

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
