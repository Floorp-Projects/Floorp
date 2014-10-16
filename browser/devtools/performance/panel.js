/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

function PerformancePanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  EventEmitter.decorate(this);
}

exports.PerformancePanel = PerformancePanel;

PerformancePanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Profiler completes opening.
   */
  open: Task.async(function*() {
    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;

    // Mock Front for now
    let gFront = {};
    EventEmitter.decorate(gFront);
    this.panelWin.gFront = gFront;

    yield this.panelWin.startupPerformance();

    this.isReady = true;
    this.emit("ready");
    return this;
  }),

  // DevToolPanel API

  get target() this._toolbox.target,

  destroy: Task.async(function*() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }

    yield this.panelWin.shutdownPerformance();
    this.emit("destroyed");
    this._destroyed = true;
  })
};
