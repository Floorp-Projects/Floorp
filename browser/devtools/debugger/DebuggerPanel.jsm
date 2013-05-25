/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["DebuggerPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");

this.DebuggerPanel = function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this._view = this.panelWin.DebuggerView;
  this._controller = this.panelWin.DebuggerController;
  this._controller._target = this.target;
  this._bkp = this._controller.Breakpoints;

  this.highlightWhenPaused = this.highlightWhenPaused.bind(this);
  this.unhighlightWhenResumed = this.unhighlightWhenResumed.bind(this);

  EventEmitter.decorate(this);
}

DebuggerPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A Promise that is resolved when the Debugger completes opening.
   */
  open: function DebuggerPanel_open() {
    let promise;

    // Local debugging needs to make the target remote.
    if (!this.target.isRemote) {
      promise = this.target.makeRemote();
    } else {
      promise = Promise.resolve(this.target);
    }

    return promise
      .then(() => this._controller.startupDebugger())
      .then(() => this._controller.connect())
      .then(() => {
        this.target.on("thread-paused", this.highlightWhenPaused);
        this.target.on("thread-resumed", this.unhighlightWhenResumed);
        this.isReady = true;
        this.emit("ready");
        return this;
      })
      .then(null, function onError(aReason) {
        Cu.reportError("DebuggerPanel open failed. " +
                       reason.error + ": " + reason.message);
      });
  },

  // DevToolPanel API
  get target() this._toolbox.target,

  destroy: function() {
    this.target.off("thread-paused", this.highlightWhenPaused);
    this.target.off("thread-resumed", this.unhighlightWhenResumed);
    this.emit("destroyed");
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

  highlightWhenPaused: function() {
    this._toolbox.highlightTool("jsdebugger");
  },

  unhighlightWhenResumed: function() {
    this._toolbox.unhighlightTool("jsdebugger");
  }
};
