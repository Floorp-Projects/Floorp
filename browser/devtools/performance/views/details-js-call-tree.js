/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * CallTree view containing profiler call tree, controlled by DetailsView.
 */
let JsCallTreeView = Heritage.extend(DetailsSubview, {

  rerenderPrefs: [
    "invert-call-tree",
    "show-platform-data"
  ],

  rangeChangeDebounceTime: 50, // ms

  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    DetailsSubview.initialize.call(this);

    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._onLink = this._onLink.bind(this);

    this.container = $("#js-calltree-view .call-tree-cells-container");

    JITOptimizationsView.initialize();
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.container = null;
    JITOptimizationsView.destroy();
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
      contentOnly: !PerformanceController.getOption("show-platform-data"),
      invertTree: PerformanceController.getOption("invert-call-tree")
    };
    let recording = PerformanceController.getCurrentRecording();
    let profile = recording.getProfile();
    let threadNode = this._prepareCallTree(profile, interval, options);
    this._populateCallTree(threadNode, options);
    this.emit(EVENTS.JS_CALL_TREE_RENDERED);
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
  _prepareCallTree: function (profile, { startTime, endTime }, options) {
    let threadSamples = profile.threads[0].samples;
    let optimizations = profile.threads[0].optimizations;
    let { contentOnly, invertTree } = options;

    let threadNode = new ThreadNode(threadSamples,
      { startTime, endTime, contentOnly, invertTree, optimizations });

    return threadNode;
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
      autoExpandDepth: inverted ? 0 : undefined
    });

    // Bind events.
    root.on("link", this._onLink);

    // Pipe "focus" events to the view, used by
    // tests and JITOptimizationsView.
    root.on("focus", (_, node) => this.emit("focus", node));

    // Clear out other call trees.
    this.container.innerHTML = "";
    root.attachTo(this.container);

    // When platform data isn't shown, hide the cateogry labels, since they're
    // only available for C++ frames.
    root.toggleCategories(options.contentOnly);

    // Return the CallView for tests
    return root;
  },

  toString: () => "[object JsCallTreeView]"
});
