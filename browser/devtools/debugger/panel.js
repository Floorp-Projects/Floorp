/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const EventEmitter = require("devtools/toolkit/event-emitter");
const { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._destroyer = null;

  this._view = this.panelWin.DebuggerView;
  this._controller = this.panelWin.DebuggerController;
  this._view._hostType = this._toolbox.hostType;
  this._controller._target = this.target;
  this._controller._toolbox = this._toolbox;

  this.handleHostChanged = this.handleHostChanged.bind(this);
  this.highlightWhenPaused = this.highlightWhenPaused.bind(this);
  this.unhighlightWhenResumed = this.unhighlightWhenResumed.bind(this);

  EventEmitter.decorate(this);
};

exports.DebuggerPanel = DebuggerPanel;

DebuggerPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Debugger completes opening.
   */
  open: function() {
    let targetPromise;

    // Local debugging needs to make the target remote.
    if (!this.target.isRemote) {
      targetPromise = this.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.target);
    }

    return targetPromise
      .then(() => this._controller.startupDebugger())
      .then(() => this._controller.connect())
      .then(() => {
        this._toolbox.on("host-changed", this.handleHostChanged);
        this.target.on("thread-paused", this.highlightWhenPaused);
        this.target.on("thread-resumed", this.unhighlightWhenResumed);
        this.isReady = true;
        this.emit("ready");
        return this;
      })
      .then(null, function onError(aReason) {
        DevToolsUtils.reportException("DebuggerPanel.prototype.open", aReason);
      });
  },

  // DevToolPanel API

  get target() this._toolbox.target,

  destroy: function() {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    this.target.off("thread-paused", this.highlightWhenPaused);
    this.target.off("thread-resumed", this.unhighlightWhenResumed);

    return this._destroyer = this._controller.shutdownDebugger().then(() => {
      this.emit("destroyed");
    });
  },

  // DebuggerPanel API

  addBreakpoint: function(aLocation, aOptions) {
    return this._controller.Breakpoints.addBreakpoint(aLocation, aOptions);
  },

  removeBreakpoint: function(aLocation) {
    return this._controller.Breakpoints.removeBreakpoint(aLocation);
  },

  handleHostChanged: function() {
    this._view.handleHostChanged(this._toolbox.hostType);
  },

  highlightWhenPaused: function() {
    this._toolbox.highlightTool("jsdebugger");

    // Also raise the toolbox window if it is undocked or select the
    // corresponding tab when toolbox is docked.
    this._toolbox.raise();
  },

  unhighlightWhenResumed: function() {
    this._toolbox.unhighlightTool("jsdebugger");
  }
};
