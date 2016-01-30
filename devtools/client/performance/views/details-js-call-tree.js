/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../performance-controller.js */
/* import-globals-from ../performance-view.js */
"use strict";

/**
 * CallTree view containing profiler call tree, controlled by DetailsView.
 */
var JsCallTreeView = Heritage.extend(DetailsSubview, {

  rerenderPrefs: [
    "invert-call-tree",
    "show-platform-data",
    "flatten-tree-recursion",
  ],

  rangeChangeDebounceTime: 75, // ms

  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    DetailsSubview.initialize.call(this);

    this._onLink = this._onLink.bind(this);
    this._onFocus = this._onFocus.bind(this);

    this.container = $("#js-calltree-view .call-tree-cells-container");

    this.optimizationsElement = $("#jit-optimizations-view");
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    ReactDOM.unmountComponentAtNode(this.optimizationsElement);
    this.optimizationsElement = null;
    this.container = null;
    this.threadNode = null;
    DetailsSubview.destroy.call(this);
  },

  /**
   * Method for handling all the set up for rendering a new call tree.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    let recording = PerformanceController.getCurrentRecording();
    let profile = recording.getProfile();
    let optimizations = recording.getConfiguration().withJITOptimizations;

    let options = {
      contentOnly: !PerformanceController.getOption("show-platform-data"),
      invertTree: PerformanceController.getOption("invert-call-tree"),
      flattenRecursion: PerformanceController.getOption("flatten-tree-recursion"),
      showOptimizationHint: optimizations
    };
    let threadNode = this.threadNode = this._prepareCallTree(profile, interval, options);
    this._populateCallTree(threadNode, options);

    if (optimizations) {
      this.showOptimizations();
    } else {
      this.hideOptimizations();
    }

    this.emit(EVENTS.JS_CALL_TREE_RENDERED);
  },

  showOptimizations: function () {
    this.optimizationsElement.classList.remove("hidden");
  },

  hideOptimizations: function () {
    this.optimizationsElement.classList.add("hidden");
  },

  _onFocus: function (_, treeItem) {
    let recording = PerformanceController.getCurrentRecording();
    let frameNode = treeItem.frame;

    if (!frameNode) {
      console.warn("No frame found!");
      return;
    }

    let frameData = frameNode.getInfo();
    let optimizationSites = frameNode.hasOptimizations()
                            ? frameNode.getOptimizations().optimizationSites
                            : [];

    let optimizations = JITOptimizationsView({
      frameData,
      optimizationSites,
      onViewSourceInDebugger: (url, line) => {
        gToolbox.viewSourceInDebugger(url, line).then(success => {
          if (success) {
            this.emit(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
          } else {
            this.emit(EVENTS.SOURCE_NOT_FOUND_IN_JS_DEBUGGER);
          }
        });
      }
    });

    ReactDOM.render(optimizations, this.optimizationsElement);

    this.emit("focus", treeItem);
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
    let thread = profile.threads[0];
    let { contentOnly, invertTree, flattenRecursion } = options;
    let threadNode = new ThreadNode(thread, { startTime, endTime, contentOnly, invertTree, flattenRecursion });

    // Real profiles from nsProfiler (i.e. not synthesized from allocation
    // logs) always have a (root) node. Go down one level in the uninverted
    // view to avoid displaying both the synthesized root node and the (root)
    // node from the profiler.
    if (!invertTree) {
      threadNode.calls = threadNode.calls[0].calls;
    }

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
      // The synthesized root node is hidden in inverted call trees.
      hidden: inverted,
      // Call trees should only auto-expand when not inverted. Passing undefined
      // will default to the CALL_TREE_AUTO_EXPAND depth.
      autoExpandDepth: inverted ? 0 : undefined,
      showOptimizationHint: options.showOptimizationHint
    });

    // Bind events.
    root.on("link", this._onLink);
    root.on("focus", this._onFocus);

    // Clear out other call trees.
    this.container.innerHTML = "";
    root.attachTo(this.container);

    // When platform data isn't shown, hide the cateogry labels, since they're
    // only available for C++ frames. Pass *false* to make them invisible.
    root.toggleCategories(!options.contentOnly);

    // Return the CallView for tests
    return root;
  },

  toString: () => "[object JsCallTreeView]"
});

EventEmitter.decorate(JsCallTreeView);
