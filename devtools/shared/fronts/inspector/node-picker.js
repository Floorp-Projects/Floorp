/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * Client-side NodePicker module.
 * To be used by inspector front when it needs to select DOM elements.
 */

/**
 * Get the NodePicker instance for an inspector front.
 * The NodePicker wraps the highlighter so that it can interact with the
 * walkerFront and selection api. The nodeFront is stateless, with the
 * HighlighterFront managing it's own state.
 *
 * @param {highlighter} highlighterFront
 * @param {walker} walkerFront
 * @return {Object} the NodePicker public API
 */
class NodePicker extends EventEmitter {
  constructor(highlighter, walker) {
    super();
    this.highlighter = highlighter;
    this.walker = walker;

    this.cancel = this.cancel.bind(this);
    this.start = this.start.bind(this);
    this.stop = this.stop.bind(this);
    this.togglePicker = this.togglePicker.bind(this);

    this._onHovered = this._onHovered.bind(this);
    this._onPicked = this._onPicked.bind(this);
    this._onPreviewed = this._onPreviewed.bind(this);
    this._onCanceled = this._onCanceled.bind(this);
  }

  /**
   * Start/stop the element picker on the debuggee target.
   * @param {Boolean} doFocus - Optionally focus the content area once the picker is
   *                            activated.
   * @return Promise that resolves when done
   */
  togglePicker(doFocus) {
    if (this.highlighter.isPicking) {
      return this.stop();
    }
    return this.start(doFocus);
  }

  /**
   * Start the element picker on the debuggee target.
   * This will request the inspector actor to start listening for mouse events
   * on the target page to highlight the hovered/picked element.
   * Depending on the server-side capabilities, this may fire events when nodes
   * are hovered.
   * @param {Boolean} doFocus - Optionally focus the content area once the picker is
   *                            activated.
   * @return Promise that resolves when the picker has started or immediately
   * if it is already started
   */
  async start(doFocus) {
    if (this.highlighter.isPicking) {
      return null;
    }
    this.emit("picker-starting");
    this.walker.on("picker-node-hovered", this._onHovered);
    this.walker.on("picker-node-picked", this._onPicked);
    this.walker.on("picker-node-previewed", this._onPreviewed);
    this.walker.on("picker-node-canceled", this._onCanceled);

    const picked = await this.highlighter.pick(doFocus);
    this.emit("picker-started");
    return picked;
  }

  /**
   * Stop the element picker. Note that the picker is automatically stopped when
   * an element is picked
   * @return Promise that resolves when the picker has stopped or immediately
   * if it is already stopped
   */
  async stop() {
    if (!this.highlighter.isPicking) {
      return;
    }
    await this.highlighter.cancelPick();
    this.walker.off("picker-node-hovered", this._onHovered);
    this.walker.off("picker-node-picked", this._onPicked);
    this.walker.off("picker-node-previewed", this._onPreviewed);
    this.walker.off("picker-node-canceled", this._onCanceled);
    this.emit("picker-stopped");
  }

  /**
   * Stop the picker, but also emit an event that the picker was canceled.
   */
  async cancel() {
    await this.stop();
    this.emit("picker-node-canceled");
  }

  /**
   * When a node is hovered by the mouse when the highlighter is in picker mode
   * @param {Object} data Information about the node being hovered
   */
  _onHovered(data) {
    this.emit("picker-node-hovered", data.node);
  }

  /**
   * When a node has been picked while the highlighter is in picker mode
   * @param {Object} data Information about the picked node
   */
  _onPicked(data) {
    this.emit("picker-node-picked", data.node);
    return this.stop();
  }

  /**
   * When a node has been shift-clicked (previewed) while the highlighter is in
   * picker mode
   * @param {Object} data Information about the picked node
   */
  _onPreviewed(data) {
    this.emit("picker-node-previewed", data.node);
  }

  /**
   * When the picker is canceled, stop the picker, and make sure the toolbox
   * gets the focus.
   */
  _onCanceled() {
    return this.cancel();
  }
}

exports.NodePicker = NodePicker;
