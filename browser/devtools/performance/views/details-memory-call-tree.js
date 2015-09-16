/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * CallTree view containing memory allocation sites, controlled by DetailsView.
 */
var MemoryCallTreeView = Heritage.extend(DetailsSubview, {

  rerenderPrefs: [
    "invert-call-tree"
  ],

  rangeChangeDebounceTime: 100, // ms

  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    DetailsSubview.initialize.call(this);

    this._onLink = this._onLink.bind(this);

    this.container = $("#memory-calltree-view > .call-tree-cells-container");
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    DetailsSubview.destroy.call(this);
  },

  /**
   * Method for handling all the set up for rendering a new call tree.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    let options = {
      invertTree: PerformanceController.getOption("invert-call-tree")
    };
    let recording = PerformanceController.getCurrentRecording();
    let allocations = recording.getAllocations();
    let threadNode = this._prepareCallTree(allocations, interval, options);
    this._populateCallTree(threadNode, options);
    this.emit(EVENTS.MEMORY_CALL_TREE_RENDERED);
  },

  /**
   * Fired on the "link" event for the call tree in this container.
   */
  _onLink: function (_, treeItem) {
    let { url, line } = treeItem.frame.getInfo();
    gToolbox.viewSourceInDebugger(url, line).then(success => {
      if (success) {
        this.emit(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
      } else {
        this.emit(EVENTS.SOURCE_NOT_FOUND_IN_JS_DEBUGGER);
      }
    });
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the call tree.
   */
  _prepareCallTree: function (allocations, { startTime, endTime }, options) {
    let thread = RecordingUtils.getProfileThreadFromAllocations(allocations);
    let { invertTree } = options;

    return new ThreadNode(thread, { startTime, endTime, invertTree });
  },

  /**
   * Renders the call tree.
   */
  _populateCallTree: function (frameNode, options={}) {
    // If we have an empty profile (no samples), then don't invert the tree, as
    // it would hide the root node and a completely blank call tree space can be
    // mis-interpreted as an error.
    let inverted = options.invertTree && frameNode.samples > 0;

    let root = new CallView({
      frame: frameNode,
      inverted: inverted,
      // Root nodes are hidden in inverted call trees.
      hidden: inverted,
      // Call trees should only auto-expand when not inverted. Passing undefined
      // will default to the CALL_TREE_AUTO_EXPAND depth.
      autoExpandDepth: inverted ? 0 : undefined,
      // Some cells like the time duration and cost percentage don't make sense
      // for a memory allocations call tree.
      visibleCells: {
        selfCount: true,
        count: true,
        selfSize: true,
        size: true,
        selfCountPercentage: true,
        countPercentage: true,
        selfSizePercentage: true,
        sizePercentage: true,
        function: true
      }
    });

    // Bind events.
    root.on("link", this._onLink);

    // Pipe "focus" events to the view, mostly for tests
    root.on("focus", () => this.emit("focus"));

    // Clear out other call trees.
    this.container.innerHTML = "";
    root.attachTo(this.container);

    // Memory allocation samples don't contain cateogry labels.
    root.toggleCategories(false);
  },

  toString: () => "[object MemoryCallTreeView]"
});

EventEmitter.decorate(MemoryCallTreeView);
