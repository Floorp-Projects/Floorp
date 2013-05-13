/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["NetMonitorPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");

this.NetMonitorPanel = function NetMonitorPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this._view = this.panelWin.NetMonitorView;
  this._controller = this.panelWin.NetMonitorController;
  this._controller._target = this.target;

  EventEmitter.decorate(this);
}

NetMonitorPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A Promise that is resolved when the NetMonitor completes opening.
   */
  open: function() {
    let promise;

    // Local monitoring needs to make the target remote.
    if (!this.target.isRemote) {
      promise = this.target.makeRemote();
    } else {
      promise = Promise.resolve(this.target);
    }

    return promise
      .then(() => this._controller.startupNetMonitor())
      .then(() => this._controller.connect())
      .then(() => {
        this.isReady = true;
        this.emit("ready");
        return this;
      })
      .then(null, function onError(aReason) {
        Cu.reportError("NetMonitorPanel open failed. " +
                       reason.error + ": " + reason.message);
      });
  },

  // DevToolPanel API
  get target() this._toolbox.target,

  destroy: function() {
    this._controller.shutdownNetMonitor().then(() => this.emit("destroyed"));
  }
};
