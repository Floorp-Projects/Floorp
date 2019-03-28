/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, $$, PerformanceController, OverviewView */
"use strict";

const { extend } = require("devtools/shared/extend");

const EVENTS = require("../events");
const { ThreadNode } = require("../modules/logic/tree-model");
const { CallView } = require("../modules/widgets/tree-view");
const { DetailsSubview } = require("./details-abstract-subview");

const RecordingUtils = require("devtools/shared/performance/recording-utils");
const EventEmitter = require("devtools/shared/event-emitter");

/**
 * CallTree view containing memory allocation sites, controlled by DetailsView.
 */
const MemoryCallTreeView = extend(DetailsSubview, {

  rerenderPrefs: [
    "invert-call-tree",
  ],

  // Units are in milliseconds.
  rangeChangeDebounceTime: 100,

  /**
   * Sets up the view with event binding.
   */
  initialize: function() {
    DetailsSubview.initialize.call(this);

    this._onLink = this._onLink.bind(this);

    this.container = $("#memory-calltree-view > .call-tree-cells-container");
  },

  /**
   * Unbinds events.
   */
  destroy: function() {
    DetailsSubview.destroy.call(this);
  },

  /**
   * Method for handling all the set up for rendering a new call tree.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function(interval = {}) {
    const options = {
      invertTree: PerformanceController.getOption("invert-call-tree"),
    };
    const recording = PerformanceController.getCurrentRecording();
    const allocations = recording.getAllocations();
    const threadNode = this._prepareCallTree(allocations, interval, options);
    this._populateCallTree(threadNode, options);
    this.emit(EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  },

  /**
   * Fired on the "link" event for the call tree in this container.
   */
  _onLink: function(treeItem) {
    const { url, line, column } = treeItem.frame.getInfo();
    PerformanceController.toolbox.viewSourceInDebugger(url, line, column)
    .then(success => {
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
  _prepareCallTree: function(allocations, { startTime, endTime }, options) {
    const thread = RecordingUtils.getProfileThreadFromAllocations(allocations);
    const { invertTree } = options;

    return new ThreadNode(thread, { startTime, endTime, invertTree });
  },

  /**
   * Renders the call tree.
   */
  _populateCallTree: function(frameNode, options = {}) {
    // If we have an empty profile (no samples), then don't invert the tree, as
    // it would hide the root node and a completely blank call tree space can be
    // mis-interpreted as an error.
    const inverted = options.invertTree && frameNode.samples > 0;

    const root = new CallView({
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
        function: true,
      },
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

  toString: () => "[object MemoryCallTreeView]",
});

EventEmitter.decorate(MemoryCallTreeView);

exports.MemoryCallTreeView = MemoryCallTreeView;
