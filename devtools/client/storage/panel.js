/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "StorageUI", "devtools/client/storage/ui", true);

class StoragePanel {
  constructor(panelWin, toolbox) {
    EventEmitter.decorate(this);

    this._toolbox = toolbox;
    this._panelWin = panelWin;

    this.destroy = this.destroy.bind(this);
  }

  get panelWindow() {
    return this._panelWin;
  }

  /**
   * open is effectively an asynchronous constructor
   */
  async open() {
    this.UI = new StorageUI(this._panelWin, this._toolbox);

    await this.UI.init();

    this.isReady = true;
    this.emit("ready");

    return this;
  }

  /**
   * Destroy the storage inspector.
   */
  destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this.UI.destroy();
    this.UI = null;

    this._toolbox = null;
    this._panelWin = null;
  }
}

exports.StoragePanel = StoragePanel;
