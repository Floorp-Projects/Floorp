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
 * @param {TargetList} targetList
 *        The TargetList component referencing all the targets to be debugged
 * @param {Selection} selection
 *        The global Selection object
 */
class NodePicker extends EventEmitter {
  constructor(targetList, selection) {
    super();

    this.targetList = targetList;
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

    // Get all the inspector fronts where the picker should start, and cache them locally
    // so we can stop the picker when needed for the same list of inspector fronts.
    this._currentInspectorFronts = await this.targetList.getAllFronts(
      this.targetList.TYPES.FRAME,
      "inspector"
    );

    for (const { walker } of this._currentInspectorFronts) {
      walker.on("picker-node-hovered", this._onHovered);
      walker.on("picker-node-picked", this._onPicked);
      walker.on("picker-node-previewed", this._onPreviewed);
      walker.on("picker-node-canceled", this._onCanceled);
      await walker.pick(doFocus);
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

    for (const { walker } of this._currentInspectorFronts) {
      walker.off("picker-node-hovered", this._onHovered);
      walker.off("picker-node-picked", this._onPicked);
      walker.off("picker-node-previewed", this._onPreviewed);
      walker.off("picker-node-canceled", this._onCanceled);
      await walker.cancelPick();
    }

    this._currentInspectorFronts = [];

    this.emit("picker-stopped");
  }

  /**
   * Stop the picker, but also emit an event that the picker was canceled.
   */
  async cancel() {
    // TODO: Remove once migrated to process-agnostic box model highlighter (Bug 1646028)
    Promise.all(
      this._currentInspectorFronts.map(({ highlighter }) =>
        highlighter.hideBoxModel()
      )
    ).catch(e => console.error);
    await this.stop();
    this.emit("picker-node-canceled");
  }

  /**
   * When a node is hovered by the mouse when the highlighter is in picker mode
   *
   * @param {Object} data
   *        Information about the node being hovered
   */
  async _onHovered(data) {
    this.emit("picker-node-hovered", data.node);

    // TODO: Remove once migrated to process-agnostic box model highlighter (Bug 1646028)
    await data.node.highlighterFront.showBoxModel(data.node);

    // One of the HighlighterActor instances, in one of the current targets, is hovering
    // over a node. Because we may be connected to several targets, we have several
    // HighlighterActor instances running at the same time. Tell the ones that don't match
    // the hovered node to hide themselves to avoid having several highlighters visible at
    // the same time.
    const unmatchedInspectors = this._currentInspectorFronts.filter(
      ({ highlighter }) => highlighter !== data.node.highlighterFront
    );

    Promise.all(
      unmatchedInspectors.map(({ highlighter }) => highlighter.hideBoxModel())
    ).catch(e => console.error);
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
  async _onPreviewed(data) {
    this.emit("picker-node-previewed", data.node);

    // TODO: Remove once migrated to process-agnostic box model highlighter (Bug 1646028)
    await data.node.highlighterFront.showBoxModel(data.node);
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
