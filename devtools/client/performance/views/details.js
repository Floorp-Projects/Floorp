/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, $$, PerformanceController */

"use strict";

const EVENTS = require("devtools/client/performance/events");

const {
  WaterfallView,
} = require("devtools/client/performance/views/details-waterfall");
const {
  JsCallTreeView,
} = require("devtools/client/performance/views/details-js-call-tree");
const {
  JsFlameGraphView,
} = require("devtools/client/performance/views/details-js-flamegraph");
const {
  MemoryCallTreeView,
} = require("devtools/client/performance/views/details-memory-call-tree");
const {
  MemoryFlameGraphView,
} = require("devtools/client/performance/views/details-memory-flamegraph");

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * Details view containing call trees, flamegraphs and markers waterfall.
 * Manages subviews and toggles visibility between them.
 */
const DetailsView = {
  /**
   * Name to (node id, view object, actor requirements, pref killswitch)
   * mapping of subviews.
   */
  components: {
    waterfall: {
      id: "waterfall-view",
      view: WaterfallView,
      features: ["withMarkers"],
    },
    "js-calltree": {
      id: "js-profile-view",
      view: JsCallTreeView,
    },
    "js-flamegraph": {
      id: "js-flamegraph-view",
      view: JsFlameGraphView,
    },
    "memory-calltree": {
      id: "memory-calltree-view",
      view: MemoryCallTreeView,
      features: ["withAllocations"],
    },
    "memory-flamegraph": {
      id: "memory-flamegraph-view",
      view: MemoryFlameGraphView,
      features: ["withAllocations"],
      prefs: ["enable-memory-flame"],
    },
  },

  /**
   * Sets up the view with event binding, initializes subviews.
   */
  async initialize() {
    this.el = $("#details-pane");
    this.toolbar = $("#performance-toolbar-controls-detail-views");

    this._onViewToggle = this._onViewToggle.bind(this);
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(
      this
    );
    this.setAvailableViews = this.setAvailableViews.bind(this);

    for (const button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.addEventListener("command", this._onViewToggle);
    }

    await this.setAvailableViews();

    PerformanceController.on(
      EVENTS.RECORDING_STATE_CHANGE,
      this._onRecordingStoppedOrSelected
    );
    PerformanceController.on(
      EVENTS.RECORDING_SELECTED,
      this._onRecordingStoppedOrSelected
    );
    PerformanceController.on(EVENTS.PREF_CHANGED, this.setAvailableViews);
  },

  /**
   * Unbinds events, destroys subviews.
   */
  async destroy() {
    for (const button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.removeEventListener("command", this._onViewToggle);
    }

    for (const component of Object.values(this.components)) {
      component.initialized && (await component.view.destroy());
    }

    PerformanceController.off(
      EVENTS.RECORDING_STATE_CHANGE,
      this._onRecordingStoppedOrSelected
    );
    PerformanceController.off(
      EVENTS.RECORDING_SELECTED,
      this._onRecordingStoppedOrSelected
    );
    PerformanceController.off(EVENTS.PREF_CHANGED, this.setAvailableViews);
  },

  /**
   * Sets the possible views based off of recording features and server actor support
   * by hiding/showing the buttons that select them and going to default view
   * if currently selected. Called when a preference changes in
   * `devtools.performance.ui.`.
   */
  async setAvailableViews() {
    const recording = PerformanceController.getCurrentRecording();
    const isCompleted = recording && recording.isCompleted();
    let invalidCurrentView = false;

    for (const [name, { view }] of Object.entries(this.components)) {
      const isSupported = this._isViewSupported(name);

      $(`toolbarbutton[data-view=${name}]`).hidden = !isSupported;

      // If the view is currently selected and not supported, go back to the
      // default view.
      if (!isSupported && this.isViewSelected(view)) {
        invalidCurrentView = true;
      }
    }

    // Two scenarios in which we select the default view.
    //
    // 1: If we currently have selected a view that is no longer valid due
    // to feature support, and this isn't the first view, and the current recording
    // is completed.
    //
    // 2. If we have a finished recording and no panel was selected yet,
    // use a default now that we have the recording configurations
    if (
      (this._initialized && isCompleted && invalidCurrentView) ||
      (!this._initialized && isCompleted && recording)
    ) {
      await this.selectDefaultView();
    }
  },

  /**
   * Takes a view name and determines if the current recording
   * can support the view.
   *
   * @param {string} viewName
   * @return {boolean}
   */
  _isViewSupported: function(viewName) {
    const { features, prefs } = this.components[viewName];
    const recording = PerformanceController.getCurrentRecording();

    if (!recording || !recording.isCompleted()) {
      return false;
    }

    const prefSupported = prefs?.length
      ? prefs.every(p => PerformanceController.getPref(p))
      : true;
    return PerformanceController.isFeatureSupported(features) && prefSupported;
  },

  /**
   * Select one of the DetailView's subviews to be rendered,
   * hiding the others.
   *
   * @param String viewName
   *        Name of the view to be shown.
   */
  async selectView(viewName) {
    const component = this.components[viewName];
    this.el.selectedPanel = $("#" + component.id);

    await this._whenViewInitialized(component);

    for (const button of $$("toolbarbutton[data-view]", this.toolbar)) {
      if (button.getAttribute("data-view") === viewName) {
        button.setAttribute("checked", true);
      } else {
        button.removeAttribute("checked");
      }
    }

    // Set a flag indicating that a view was explicitly set based on a
    // recording's features.
    this._initialized = true;

    this.emit(EVENTS.UI_DETAILS_VIEW_SELECTED, viewName);
  },

  /**
   * Selects a default view based off of protocol support
   * and preferences enabled.
   */
  selectDefaultView: function() {
    // We want the waterfall to be default view in almost all cases, except when
    // timeline actor isn't supported, or we have markers disabled (which should only
    // occur temporarily via bug 1156499
    if (this._isViewSupported("waterfall")) {
      return this.selectView("waterfall");
    }
    // The JS CallTree should always be supported since the profiler
    // actor is as old as the world.
    return this.selectView("js-calltree");
  },

  /**
   * Checks if the provided view is currently selected.
   *
   * @param object viewObject
   * @return boolean
   */
  isViewSelected: function(viewObject) {
    // If not initialized, and we have no recordings,
    // no views are selected (even though there's a selected panel)
    if (!this._initialized) {
      return false;
    }

    const selectedPanel = this.el.selectedPanel;
    const selectedId = selectedPanel.id;

    for (const { id, view } of Object.values(this.components)) {
      if (id == selectedId && view == viewObject) {
        return true;
      }
    }

    return false;
  },

  /**
   * Initializes a subview if it wasn't already set up, and makes sure
   * it's populated with recording data if there is some available.
   *
   * @param object component
   *        A component descriptor from DetailsView.components
   */
  async _whenViewInitialized(component) {
    if (component.initialized) {
      return;
    }
    component.initialized = true;
    await component.view.initialize();

    // If this view is initialized *after* a recording is shown, it won't display
    // any data. Make sure it's populated by setting `shouldUpdateWhenShown`.
    // All detail views require a recording to be complete, so do not
    // attempt to render if recording is in progress or does not exist.
    const recording = PerformanceController.getCurrentRecording();
    if (recording && recording.isCompleted()) {
      component.view.shouldUpdateWhenShown = true;
    }
  },

  /**
   * Called when recording stops or is selected.
   */
  _onRecordingStoppedOrSelected: function(state, recording) {
    if (typeof state === "string" && state !== "recording-stopped") {
      return;
    }
    this.setAvailableViews();
  },

  /**
   * Called when a view button is clicked.
   */
  _onViewToggle: function(e) {
    this.selectView(e.target.getAttribute("data-view"));
  },

  toString: () => "[object DetailsView]",
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(DetailsView);

exports.DetailsView = DetailsView;
