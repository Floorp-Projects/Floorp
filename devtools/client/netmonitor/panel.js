/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const { Task } = require("devtools/shared/task");
const { localizeMarkup } = require("devtools/shared/l10n");

function NetMonitorPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.panelDoc = iframeWindow.document;
  this._toolbox = toolbox;

  this._view = this.panelWin.NetMonitorView;
  this._controller = this.panelWin.NetMonitorController;
  this._controller._target = this.target;
  this._controller._toolbox = this._toolbox;

  EventEmitter.decorate(this);
}

exports.NetMonitorPanel = NetMonitorPanel;

NetMonitorPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the NetMonitor completes opening.
   */
  open: Task.async(function* () {
    if (this._opening) {
      return this._opening;
    }
    // Localize all the nodes containing a data-localization attribute.
    localizeMarkup(this.panelDoc);

    let deferred = promise.defer();
    this._opening = deferred.promise;

    // Local monitoring needs to make the target remote.
    if (!this.target.isRemote) {
      yield this.target.makeRemote();
    }

    yield this._controller.startupNetMonitor();
    this.isReady = true;
    this.emit("ready");

    deferred.resolve(this);
    return this._opening;
  }),

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: Task.async(function* () {
    if (this._destroying) {
      return this._destroying;
    }
    let deferred = promise.defer();
    this._destroying = deferred.promise;

    yield this._controller.shutdownNetMonitor();
    this.emit("destroyed");

    deferred.resolve();
    return this._destroying;
  })
};
