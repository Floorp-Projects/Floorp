/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Details view containing call trees, flamegraphs and markers waterfall.
 * Manages subviews and toggles visibility between them.
 */
let DetailsView = {
  /**
   * Name to (node id, view object, actor requirements, pref killswitch)
   * mapping of subviews.
   */
  components: {
    "waterfall": {
      id: "waterfall-view",
      view: WaterfallView,
      requires: ["timeline"]
    },
    "js-calltree": {
      id: "js-profile-view",
      view: JsCallTreeView
    },
    "js-flamegraph": {
      id: "js-flamegraph-view",
      view: JsFlameGraphView,
      requires: ["timeline"]
    },
    "memory-calltree": {
      id: "memory-calltree-view",
      view: MemoryCallTreeView,
      requires: ["memory"],
      pref: "enable-memory"
    },
    "memory-flamegraph": {
      id: "memory-flamegraph-view",
      view: MemoryFlameGraphView,
      requires: ["memory", "timeline"],
      pref: "enable-memory"
    }
  },

  /**
   * Sets up the view with event binding, initializes subviews.
   */
  initialize: Task.async(function *() {
    this.el = $("#details-pane");
    this.toolbar = $("#performance-toolbar-controls-detail-views");

    this._onViewToggle = this._onViewToggle.bind(this);
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(this);
    this.setAvailableViews = this.setAvailableViews.bind(this);

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.addEventListener("command", this._onViewToggle);
    }

    yield this.selectDefaultView();
    yield this.setAvailableViews();

    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.PREF_CHANGED, this.setAvailableViews);
  }),

  /**
   * Unbinds events, destroys subviews.
   */
  destroy: Task.async(function *() {
    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.removeEventListener("command", this._onViewToggle);
    }

    for (let [_, component] of Iterator(this.components)) {
      component.initialized && (yield component.view.destroy());
    }

    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.PREF_CHANGED, this.setAvailableViews);
  }),

  /**
   * Sets the possible views based off of prefs and server actor support by hiding/showing the
   * buttons that select them and going to default view if currently selected.
   * Called when a preference changes in `devtools.performance.ui.`.
   */
  setAvailableViews: Task.async(function* () {
    let mocks = gFront.getMocksInUse();

    for (let [name, { view, pref, requires }] of Iterator(this.components)) {
      let recording = PerformanceController.getCurrentRecording();

      let isRecorded = recording && !recording.isRecording();
      // View is enabled by its corresponding pref
      let isEnabled = !pref || PerformanceController.getOption(pref);
      // View is supported by the server actor, and the requried actor is not being mocked
      let isSupported = !requires || requires.every(r => !mocks[r]);

      $(`toolbarbutton[data-view=${name}]`).hidden = !isRecorded || !(isEnabled && isSupported);

      // If the view is currently selected and not enabled, go back to the
      // default view.
      if (!isEnabled && this.isViewSelected(view)) {
        yield this.selectDefaultView();
      }
    }
  }),

  /**
   * Select one of the DetailView's subviews to be rendered,
   * hiding the others.
   *
   * @param String viewName
   *        Name of the view to be shown.
   */
  selectView: Task.async(function *(viewName) {
    let component = this.components[viewName];
    this.el.selectedPanel = $("#" + component.id);

    yield this._whenViewInitialized(component);

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      if (button.getAttribute("data-view") === viewName) {
        button.setAttribute("checked", true);
      } else {
        button.removeAttribute("checked");
      }
    }

    this.emit(EVENTS.DETAILS_VIEW_SELECTED, viewName);
  }),

  /**
   * Selects a default view based off of protocol support
   * and preferences enabled.
   */
  selectDefaultView: function () {
    let { timeline: mockTimeline } = gFront.getMocksInUse();
    // If timelines are mocked, the first view available is the js-calltree.
    if (mockTimeline) {
      return this.selectView("js-calltree");
    } else {
      // In every other scenario with preferences and mocks, waterfall will
      // be the default view.
      return this.selectView("waterfall");
    }
  },

  /**
   * Checks if the provided view is currently selected.
   *
   * @param object viewObject
   * @return boolean
   */
  isViewSelected: function(viewObject) {
    let selectedPanel = this.el.selectedPanel;
    let selectedId = selectedPanel.id;

    for (let [, { id, view }] of Iterator(this.components)) {
      if (id == selectedId && view == viewObject) {
        return true;
      }
    }

    return false;
  },

  /**
   * Resolves when the provided view is selected. If already selected,
   * the returned promise resolves immediately.
   *
   * @param object viewObject
   * @return object
   */
  whenViewSelected: Task.async(function*(viewObject) {
    if (this.isViewSelected(viewObject)) {
      return promise.resolve();
    }
    yield this.once(EVENTS.DETAILS_VIEW_SELECTED);
    return this.whenViewSelected(viewObject);
  }),

  /**
   * Initializes a subview if it wasn't already set up, and makes sure
   * it's populated with recording data if there is some available.
   *
   * @param object component
   *        A component descriptor from DetailsView.components
   */
  _whenViewInitialized: Task.async(function *(component) {
    if (component.initialized) {
      return;
    }
    component.initialized = true;
    yield component.view.initialize();

    // If this view is initialized *after* a recording is shown, it won't display
    // any data. Make sure it's populated by setting `shouldUpdateWhenShown`.
    // All detail views require a recording to be complete, so do not
    // attempt to render if recording is in progress or does not exist.
    let recording = PerformanceController.getCurrentRecording();
    if (recording && !recording.isRecording()) {
      component.view.shouldUpdateWhenShown = true;
    }
  }),

  /**
   * Called when recording stops or is selected.
   */
  _onRecordingStoppedOrSelected: function(_, recording) {
    this.setAvailableViews();
  },

  /**
   * Called when a view button is clicked.
   */
  _onViewToggle: function (e) {
    this.selectView(e.target.getAttribute("data-view"));
  },

  toString: () => "[object DetailsView]"
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(DetailsView);
