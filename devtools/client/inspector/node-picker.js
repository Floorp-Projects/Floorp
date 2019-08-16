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
 * @param {Target} target
 *        The target the toolbox will debug
 * @param {Selection} selection
 *        The global Selection object
 */
class NodePicker extends EventEmitter {
  constructor(target, selection) {
    super();

    this.target = target;
    this.selection = selection;

    // Whether or not the node picker is active.
    this.isPicking = false;

    // The list of inspector fronts corresponding to the frames where picking happens.
    this._currentInspectorFronts = [];

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
   * Get all of the InspectorFront instances corresponding to the frames where the node
   * picking should occur.
   *
   * @return {Array<InspectorFront>}
   *         The list of InspectorFront instances
   */
  async getAllInspectorFronts() {
    // TODO: For Fission, we should list all remote frames here.
    // TODO: For the Browser Toolbox, we should list all remote browsers here.
    // TODO: For now we just return a single item in the array.
    const inspectorFront = await this.target.getFront("inspector");
    return [inspectorFront];
  }

  /**
   * Start/stop the element picker on the debuggee target.
   *
   * @param {Boolean} doFocus
   *        Optionally focus the content area once the picker is activated.
   * @return Promise that resolves when done
   */
  togglePicker(doFocus) {
    if (this.isPicking) {
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
   *
   * @param {Boolean} doFocus
   *        Optionally focus the content area once the picker is activated.
   */
  async start(doFocus) {
    if (this.isPicking) {
      return;
    }
    this.isPicking = true;

    this.emit("picker-starting");

    this._currentInspectorFronts = await this.getAllInspectorFronts();

    for (const { walker, highlighter } of this._currentInspectorFronts) {
      walker.on("picker-node-hovered", this._onHovered);
      walker.on("picker-node-picked", this._onPicked);
      walker.on("picker-node-previewed", this._onPreviewed);
      walker.on("picker-node-canceled", this._onCanceled);

      await highlighter.pick(doFocus);
    }

    this.emit("picker-started");
  }

  /**
   * Stop the element picker. Note that the picker is automatically stopped when
   * an element is picked.
   */
  async stop() {
    if (!this.isPicking) {
      return;
    }
    this.isPicking = false;

    for (const { walker, highlighter } of this._currentInspectorFronts) {
      await highlighter.cancelPick();

      walker.off("picker-node-hovered", this._onHovered);
      walker.off("picker-node-picked", this._onPicked);
      walker.off("picker-node-previewed", this._onPreviewed);
      walker.off("picker-node-canceled", this._onCanceled);
    }

    this._currentInspectorFronts = [];

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
   *
   * @param {Object} data
   *        Information about the node being hovered
   */
  _onHovered(data) {
    this.emit("picker-node-hovered", data.node);
  }

  /**
   * When a node has been picked while the highlighter is in picker mode
   *
   * @param {Object} data
   *        Information about the picked node
   */
  _onPicked(data) {
    this.emit("picker-node-picked", data.node);
    return this.stop();
  }

  /**
   * When a node has been shift-clicked (previewed) while the highlighter is in
   * picker mode
   *
   * @param {Object} data
   *        Information about the picked node
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

module.exports = NodePicker;
