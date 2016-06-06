/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const { WebGLFront } = require("devtools/shared/fronts/webgl");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

function ShaderEditorPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._destroyer = null;

  EventEmitter.decorate(this);
}

exports.ShaderEditorPanel = ShaderEditorPanel;

ShaderEditorPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Shader Editor completes opening.
   */
  open: function () {
    let targetPromise;

    // Local debugging needs to make the target remote.
    if (!this.target.isRemote) {
      targetPromise = this.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.target);
    }

    return targetPromise
      .then(() => {
        this.panelWin.gToolbox = this._toolbox;
        this.panelWin.gTarget = this.target;
        this.panelWin.gFront = new WebGLFront(this.target.client, this.target.form);
        return this.panelWin.startupShaderEditor();
      })
      .then(() => {
        this.isReady = true;
        this.emit("ready");
        return this;
      })
      .then(null, function onError(aReason) {
        DevToolsUtils.reportException("ShaderEditorPanel.prototype.open", aReason);
      });
  },

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: function () {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    return this._destroyer = this.panelWin.shutdownShaderEditor().then(() => {
      // Destroy front to ensure packet handler is removed from client
      this.panelWin.gFront.destroy();
      this.emit("destroyed");
    });
  }
};
