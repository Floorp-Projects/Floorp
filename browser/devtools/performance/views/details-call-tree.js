/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const CALLTREE_UPDATE_DEBOUNCE = 50; // ms

/**
 * CallTree view containing profiler call tree, controlled by DetailsView.
 */
let CallTreeView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(this);
    this._onRangeChange = this._onRangeChange.bind(this);
    this._onDetailsViewSelected = this._onDetailsViewSelected.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._onLink = this._onLink.bind(this);

    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.on(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    clearNamedTimeout("calltree-update");

    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.off(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  },

  /**
   * Method for handling all the set up for rendering a new call tree.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   * @param object options [optional]
   *        Additional options for new the call tree.
   */
  render: function (interval={}, options={}) {
    let recording = PerformanceController.getCurrentRecording();
    let profile = recording.getProfile();
    let threadNode = this._prepareCallTree(profile, interval, options);
    this._populateCallTree(threadNode, options);
    this.emit(EVENTS.CALL_TREE_RENDERED);
  },

  /**
   * Called when recording is stopped or has been selected.
   */
  _onRecordingStoppedOrSelected: function (_, recording) {
    if (!recording.isRecording()) {
      this.render();
    }
  },

  /**
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onRangeChange: function (_, interval) {
    if (DetailsView.isViewSelected(this)) {
      let debounced = () => this.render(interval);
      setNamedTimeout("calltree-update", CALLTREE_UPDATE_DEBOUNCE, debounced);
    } else {
      this._dirty = true;
      this._interval = interval;
    }
  },

  /**
   * Fired when a view is selected in the DetailsView.
   */
  _onDetailsViewSelected: function() {
    if (DetailsView.isViewSelected(this) && this._dirty) {
      this.render(this._interval);
      this._dirty = false;
    }
  },

  /**
   * Fired on the "link" event for the call tree in this container.
   */
  _onLink: function (_, treeItem) {
    let { url, line } = treeItem.frame.getInfo();
    viewSourceInDebugger(url, line).then(
      () => this.emit(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER),
      () => this.emit(EVENTS.SOURCE_NOT_FOUND_IN_JS_DEBUGGER));
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the call tree.
   */
  _prepareCallTree: function (profile, { startTime, endTime }, options) {
    let threadSamples = profile.threads[0].samples;
    let contentOnly = !Prefs.showPlatformData;
    let invertTree = PerformanceController.getPref("invert-call-tree");

    let threadNode = new ThreadNode(threadSamples,
      { startTime, endTime, contentOnly, invertTree });

    // If we have an empty profile (no samples), then don't invert the tree, as
    // it would hide the root node and a completely blank call tree space can be
    // mis-interpreted as an error.
    options.inverted = invertTree && threadNode.samples > 0;

    return threadNode;
  },

  /**
   * Renders the call tree.
   */
  _populateCallTree: function (frameNode, options={}) {
    let root = new CallView({
      frame: frameNode,
      inverted: options.inverted,
      // Root nodes are hidden in inverted call trees.
      hidden: options.inverted,
      // Call trees should only auto-expand when not inverted. Passing undefined
      // will default to the CALL_TREE_AUTO_EXPAND depth.
      autoExpandDepth: options.inverted ? 0 : undefined,
    });

    // Bind events.
    root.on("link", this._onLink);

    // Clear out other call trees.
    let container = $(".call-tree-cells-container");
    container.innerHTML = "";
    root.attachTo(container);

    // When platform data isn't shown, hide the cateogry labels, since they're
    // only available for C++ frames.
    let contentOnly = !Prefs.showPlatformData;
    root.toggleCategories(!contentOnly);
  },

  /**
   * Called when a preference under "devtools.performance.ui." is changed.
   */
  _onPrefChanged: function (_, prefName, value) {
    if (prefName === "invert-call-tree") {
      this.render(OverviewView.getTimeInterval());
    }
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(CallTreeView);

/**
 * Opens/selects the debugger in this toolbox and jumps to the specified
 * file name and line number.
 * @param string url
 * @param number line
 */
let viewSourceInDebugger = Task.async(function *(url, line) {
  // If the Debugger was already open, switch to it and try to show the
  // source immediately. Otherwise, initialize it and wait for the sources
  // to be added first.
  let debuggerAlreadyOpen = gToolbox.getPanel("jsdebugger");
  let { panelWin: dbg } = yield gToolbox.selectTool("jsdebugger");

  if (!debuggerAlreadyOpen) {
    yield dbg.once(dbg.EVENTS.SOURCES_ADDED);
  }

  let { DebuggerView } = dbg;
  let { Sources } = DebuggerView;

  let item = Sources.getItemForAttachment(a => a.source.url === url);
  if (item) {
    return DebuggerView.setEditorLocation(item.attachment.source.actor, line, { noDebug: true });
  }

  return Promise.reject("Couldn't find the specified source in the debugger.");
});
