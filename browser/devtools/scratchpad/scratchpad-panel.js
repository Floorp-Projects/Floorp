/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cu} = require("chrome");
const EventEmitter = require("devtools/shared/event-emitter");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});


function ScratchpadPanel(iframeWindow, toolbox) {
  let { Scratchpad } = iframeWindow;
  this._toolbox = toolbox;
  this.panelWin = iframeWindow;
  this.scratchpad = Scratchpad;

  Scratchpad.target = this.target;
  Scratchpad.hideMenu();

  let deferred = promise.defer();
  this._readyObserver = deferred.promise;
  Scratchpad.addObserver({
    onReady: function() {
      Scratchpad.removeObserver(this);
      deferred.resolve();
    }
  });

  EventEmitter.decorate(this);
}
exports.ScratchpadPanel = ScratchpadPanel;

ScratchpadPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor. For the ScratchpadPanel,
   * by the time this is called, the Scratchpad will already be ready.
   */
  open: function() {
    return this._readyObserver.then(() => {
      this.isReady = true;
      this.emit("ready");
      return this;
    });
  },

  get target() {
    return this._toolbox.target;
  },

  destroy: function() {
    this.emit("destroyed");
    return promise.resolve();
  }
};
