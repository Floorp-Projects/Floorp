/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let OptimizationsView = Heritage.extend(DetailsSubview, {

  rerenderPrefs: [
    "show-platform-data",
    "flatten-tree-recursion",
  ],

  rangeChangeDebounceTime: 75, // ms

  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    DetailsSubview.initialize.call(this);
    this.reset = this.reset.bind(this);
    this.tabs = $("#optimizations-tabs");
    this._onFramesListSelect = this._onFramesListSelect.bind(this);

    OptimizationsListView.initialize();
    FramesListView.initialize({ container: $("#frames-tabpanel") });
    FramesListView.on("select", this._onFramesListSelect);
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    DetailsSubview.destroy.call(this);
    this.tabs = this._threadNode = this._frameNode = null;

    FramesListView.off("select", this._onFramesListSelect);
    FramesListView.destroy();
    OptimizationsListView.destroy();
  },

  /**
   * Selects a tab by name.
   *
   * @param {string} name
   *                 Can be "frames" or "optimizations"
   */
  selectTabByName: function (name="frames") {
    switch(name) {
    case "optimizations":
      this.tabs.selectedIndex = 0;
      break;
    case "frames":
      this.tabs.selectedIndex = 1;
      break;
    }
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
      flattenRecursion: PerformanceController.getOption("flatten-tree-recursion"),
      // Always invert the tree for the optimizations view so we can quickly
      // get leaves
      invertTree: true,
    };
    let recording = PerformanceController.getCurrentRecording();
    let profile = recording.getProfile();

    this.reset();
    // TODO bug 1175662
    // Share thread nodes between details view
    this.threadNode = this._prepareThreadNode(profile, interval, options);
    this.emit(EVENTS.OPTIMIZATIONS_RENDERED);
  },

  /**
   * The main thread node used in this recording that contains
   * all potential frame nodes to select.
   */
  set threadNode(threadNode) {
    if (threadNode === this._threadNode) {
      return;
    }
    this._threadNode = threadNode;
    // Also clear out the current frame node as its no
    // longer relevent
    this.frameNode = null;
    this._setAndRenderFramesList();
  },
  get threadNode() {
    return this._threadNode;
  },

  /**
   * frameNode is the frame node selected currently to inspect
   * the optimization tiers over time and strategies.
   */
  set frameNode(frameNode) {
    if (frameNode === this._frameNode) {
      return;
    }
    this._frameNode = frameNode;

    // If no frame selected, jump to the frame list view. If just selected
    // a frame, jump to optimizations view.
    // TODO test for this bug 1176056
    this.selectTabByName(frameNode ? "optimizations" : "frames");
    this._setAndRenderTierGraph();
    this._setAndRenderOptimizationsList();
  },

  get frameNode() {
    return this._frameNode;
  },

  /**
   * Clears the frameNode so that tier and opts list
   * views are cleared.
   */
  reset: function () {
    this.threadNode = this.frameNode = null;
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the graph.
   */
  _prepareThreadNode: function (profile, { startTime, endTime }, options) {
    let thread = profile.threads[0];
    let { contentOnly, invertTree, flattenRecursion } = options;
    let threadNode = new ThreadNode(thread, { startTime, endTime, contentOnly, invertTree, flattenRecursion });
    return threadNode;
  },

  /**
   * Renders the tier graph.
   */
  _setAndRenderTierGraph: function () {
    // TODO bug 1150299
  },

  /**
   * Renders the frames list.
   */
  _setAndRenderFramesList: function () {
    FramesListView.setCurrentThread(this.threadNode);
    FramesListView.render();
  },

  /**
   * Renders the optimizations list.
   */
  _setAndRenderOptimizationsList: function () {
    OptimizationsListView.setCurrentFrame(this.frameNode);
    OptimizationsListView.render();
  },

  /**
   * Called when a frame is selected via the FramesListView
   */
  _onFramesListSelect: function (_, frameNode) {
    this.frameNode = frameNode;
  },

  toString: () => "[object OptimizationsView]"
});

EventEmitter.decorate(OptimizationsView);
