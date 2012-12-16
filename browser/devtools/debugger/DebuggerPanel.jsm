/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["DebuggerPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
  "resource://gre/modules/devtools/dbg-server.jsm");

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this._controller = this.panelWin.DebuggerController;
  this._view = this.panelWin.DebuggerView;
  this._controller._target = this.target;
  this._bkp = this._controller.Breakpoints;

  new EventEmitter(this);
}

DebuggerPanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: function DebuggerPanel_open() {
    let deferred = Promise.defer();

    this._ensureOnlyOneRunningDebugger();

    if (!this.target.isRemote) {
      if (!DebuggerServer.initialized) {
        DebuggerServer.init();
        DebuggerServer.addBrowserActors();
      }
    }

    let onDebuggerLoaded = function () {
      this.panelWin.removeEventListener("Debugger:Loaded",
                                        onDebuggerLoaded, true);
      this._isReady = true;
      this.emit("ready");
      deferred.resolve(this);
    }.bind(this);

    let onDebuggerConnected = function () {
      this.panelWin.removeEventListener("Debugger:Connected",
                                        onDebuggerConnected, true);
      this.emit("connected");
    }.bind(this);

    this.panelWin.addEventListener("Debugger:Loaded", onDebuggerLoaded, true);
    this.panelWin.addEventListener("Debugger:Connected",
                                   onDebuggerConnected, true);

    return deferred.promise;
  },

  // DevToolPanel API
  get target() this._toolbox.target,

  get isReady() this._isReady,

  destroy: function() {
    return Promise.resolve(null);
  },

  // DebuggerPanel API

  addBreakpoint: function() {
    this._bkp.addBreakpoint.apply(this._bkp, arguments);
  },

  removeBreakpoint: function() {
    this._bkp.removeBreakpoint.apply(this._bkp, arguments);
  },

  getBreakpoint: function() {
    return this._bkp.getBreakpoint.apply(this._bkp, arguments);
  },

  getAllBreakpoints: function() {
    return this._bkp.store;
  },

  // Private

  _ensureOnlyOneRunningDebugger: function() {
    // FIXME
  },
};
