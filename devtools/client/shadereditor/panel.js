/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { WebGLFront } = require("devtools/shared/fronts/webgl");
const { EventsHandler, ShadersListView, ShadersEditorsView, EVENTS, $, L10N } =
  require("./shadereditor");

function ShaderEditorPanel(toolbox) {
  this._toolbox = toolbox;
  this._destroyer = null;
  this.panelWin = window;

  EventEmitter.decorate(this);
}

exports.ShaderEditorPanel = ShaderEditorPanel;

ShaderEditorPanel.prototype = {

  // Expose symbols for tests:
  EVENTS,
  $,
  L10N,

  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Shader Editor completes opening.
   */
  async open() {
    // Local debugging needs to make the target remote.
    if (!this.target.isRemote) {
      await this.target.makeRemote();
    }

    this.front = new WebGLFront(this.target.client, this.target.form);
    this.shadersListView = new ShadersListView();
    this.eventsHandler = new EventsHandler();
    this.shadersEditorsView = new ShadersEditorsView();
    await this.shadersListView.initialize(this._toolbox, this.shadersEditorsView);
    await this.eventsHandler.initialize(this, this._toolbox, this.target, this.front,
                                        this.shadersListView);
    await this.shadersEditorsView.initialize(this, this.shadersListView);

    this.isReady = true;
    this.emit("ready");
    return this;
  },

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    return (this._destroyer = (async () => {
      await this.shadersListView.destroy();
      await this.eventsHandler.destroy();
      await this.shadersEditorsView.destroy();
      // Destroy front to ensure packet handler is removed from client
      this.front.destroy();
      this.emit("destroyed");
    })());
  }
};
