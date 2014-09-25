/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");

Cu.import("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

loader.lazyRequireGetter(this, "TimelineFront",
  "devtools/server/actors/timeline", true);

function TimelinePanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  EventEmitter.decorate(this);
};

exports.TimelinePanel = TimelinePanel;

TimelinePanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the timeline completes opening.
   */
  open: Task.async(function*() {
    // Local debugging needs to make the target remote.
    yield this.target.makeRemote();

    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;
    this.panelWin.gFront = new TimelineFront(this.target.client, this.target.form);
    yield this.panelWin.startupTimeline();

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

    yield this.panelWin.shutdownTimeline();
    // Destroy front to ensure packet handler is removed from client
    this.panelWin.gFront.destroy();
    this.emit("destroyed");
    this._destroyed = true;
  })
};
