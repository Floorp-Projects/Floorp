/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * Client-side NodePicker module.
 * To be used by inspector front when it needs to select DOM elements.
 *
 * NodePicker is a proxy for the node picker functionality from WalkerFront instances
 * of all available InspectorFronts. It is a single point of entry for the client to:
 * - invoke actions to start and stop picking nodes on all walkers
 * - listen to node picker events from all walkers and relay them to subscribers
 *
 *
 * @param {TargetCommand} targetCommand
 *        The TargetCommand component referencing all the targets to be debugged
 * @param {Selection} selection
 *        The global Selection object
 */
class NodePicker extends EventEmitter {
  constructor(targetCommand, selection) {
    super();

    this.targetCommand = targetCommand;

    // Whether or not the node picker is active.
    this.isPicking = false;
    // Whether to focus the top-level frame before picking nodes.
    this.doFocus = false;

    // The set of inspector fronts corresponding to the targets where picking happens.
    this._currentInspectorFronts = new Set();

    this._onInspectorFrontAvailable = this._onInspectorFrontAvailable.bind(
      this
    );
    this._onInspectorFrontDestroyed = this._onInspectorFrontDestroyed.bind(
      this
    );
    this._onTargetAvailable = this._onTargetAvailable.bind(this);

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
   * Tell the walker front corresponding to the given inspector front to enter node
   * picking mode (listen for mouse movements over its nodes) and set event listeners
   * associated with node picking: hover node, pick node, preview, cancel. See WalkerSpec.
   *
   * @param {InspectorFront} inspectorFront
   * @return {Promise}
   */
  async _onInspectorFrontAvailable(inspectorFront) {
    this._currentInspectorFronts.add(inspectorFront);
    // watchFront may notify us about inspector fronts that aren't initialized yet,
    // so ensure waiting for initialization in order to have a defined `walker` attribute.
    await inspectorFront.initialize();
    const { walker } = inspectorFront;
    walker.on("picker-node-hovered", this._onHovered);
    walker.on("picker-node-picked", this._onPicked);
    walker.on("picker-node-previewed", this._onPreviewed);
    walker.on("picker-node-canceled", this._onCanceled);
    await walker.pick(this.doFocus);

    this.emitForTests("inspector-front-ready-for-picker", walker);
  }

  /**
   * Tell the walker front corresponding to the given inspector front to exit the node
   * picking mode and remove all event listeners associated with node picking.
   *
   * @param {InspectorFront} inspectorFront
   * @return {Promise}
   */
  async _onInspectorFrontDestroyed(inspectorFront) {
    this._currentInspectorFronts.delete(inspectorFront);

    const { walker } = inspectorFront;
    if (!walker) {
      return;
    }

    walker.off("picker-node-hovered", this._onHovered);
    walker.off("picker-node-picked", this._onPicked);
    walker.off("picker-node-previewed", this._onPreviewed);
    walker.off("picker-node-canceled", this._onCanceled);
    await walker.cancelPick();
  }

  /**
   * While node picking, we want each target's walker fronts to listen for mouse
   * movements over their nodes and emit events. Walker fronts are obtained from
   * inspector fronts so we watch for the creation and destruction of inspector fronts
   * in order to add or remove the necessary event listeners.
   *
   * @param {TargetFront} targetFront
   * @return {Promise}
   */
  async _onTargetAvailable({ targetFront }) {
    targetFront.watchFronts(
      "inspector",
      this._onInspectorFrontAvailable,
      this._onInspectorFrontDestroyed
    );
  }

  /**
   * Start the element picker.
   * This will instruct walker fronts of all available targets (and those of targets
   * created while node picking is active) to listen for mouse movements over their nodes
   * and trigger events when a node is hovered or picked.
   *
   * @param {Boolean} doFocus
   *        Optionally focus the content area once the picker is activated.
   */
  async start(doFocus) {
    if (this.isPicking) {
      return;
    }
    this.isPicking = true;
    this.doFocus = doFocus;

    this.emit("picker-starting");

    this.targetCommand.watchTargets(
      this.targetCommand.ALL_TYPES,
      this._onTargetAvailable
    );

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
    this.doFocus = false;

    this.targetCommand.unwatchTargets(
      this.targetCommand.ALL_TYPES,
      this._onTargetAvailable
    );

    for (const inspectorFront of this._currentInspectorFronts) {
      await this._onInspectorFrontDestroyed(inspectorFront);
    }

    this._currentInspectorFronts.clear();

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
