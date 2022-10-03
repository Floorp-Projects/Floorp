/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

loader.lazyRequireGetter(
  this,
  "StorageUI",
  "resource://devtools/client/storage/ui.js",
  true
);

class StoragePanel {
  constructor(panelWin, toolbox, commands) {
    EventEmitter.decorate(this);

    this._toolbox = toolbox;
    this._commands = commands;
    this._panelWin = panelWin;
  }

  get panelWindow() {
    return this._panelWin;
  }

  /**
   * open is effectively an asynchronous constructor
   */
  async open() {
    this.UI = new StorageUI(this._panelWin, this._toolbox, this._commands);

    await this.UI.init();

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
