/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DEFAULT_DETAILS_SUBVIEW = "waterfall";

/**
 * Details view containing profiler call tree and markers waterfall. Manages
 * subviews and toggles visibility between them.
 */
let DetailsView = {
  /**
   * Name to node+object mapping of subviews.
   */
  components: {
    "waterfall": { id: "waterfall-view", view: WaterfallView },
    "js-calltree": { id: "js-calltree-view", view: JsCallTreeView },
    "js-flamegraph": { id: "js-flamegraph-view", view: JsFlameGraphView },
    "memory-calltree": { id: "memory-calltree-view", view: MemoryCallTreeView, pref: "enable-memory" },
    "memory-flamegraph": { id: "memory-flamegraph-view", view: MemoryFlameGraphView, pref: "enable-memory" }
  },

  /**
   * Sets up the view with event binding, initializes subviews.
   */
  initialize: Task.async(function *() {
    this.el = $("#details-pane");
    this.toolbar = $("#performance-toolbar-controls-detail-views");

    this._onViewToggle = this._onViewToggle.bind(this);
    this.setAvailableViews = this.setAvailableViews.bind(this);

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.addEventListener("command", this._onViewToggle);
    }

    for (let [_, { view }] of Iterator(this.components)) {
      yield view.initialize();
    }

    this.selectView(DEFAULT_DETAILS_SUBVIEW);
    this.setAvailableViews();
    PerformanceController.on(EVENTS.PREF_CHANGED, this.setAvailableViews);
  }),

  /**
   * Unbinds events, destroys subviews.
   */
  destroy: Task.async(function *() {
    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.removeEventListener("command", this._onViewToggle);
    }

    for (let [_, { view }] of Iterator(this.components)) {
      yield view.destroy();
    }
    PerformanceController.off(EVENTS.PREF_CHANGED, this.setAvailableViews);
  }),

  /**
   * Sets the possible views based off of prefs by hiding/showing the
   * buttons that select them and going to default view if currently selected.
   * Called when a preference changes in `devtools.performance.ui.`.
   */
  setAvailableViews: function () {
    for (let [name, { view, pref }] of Iterator(this.components)) {
      if (!pref) {
        continue;
      }
      let value = PerformanceController.getPref(pref);
      $(`toolbarbutton[data-view=${name}]`).hidden = !value;

      // If the view is currently selected and not enabled, go back to the default view
      if (!value && this.isViewSelected(view)) {
        this.selectView(DEFAULT_DETAILS_SUBVIEW);
      }
    }
  },

  /**
   * Select one of the DetailView's subviews to be rendered,
   * hiding the others.
   *
   * @param String viewName
   *        Name of the view to be shown.
   */
  selectView: function (viewName) {
    this.el.selectedPanel = $("#" + this.components[viewName].id);

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      if (button.getAttribute("data-view") === viewName) {
        button.setAttribute("checked", true);
      } else {
        button.removeAttribute("checked");
      }
    }

    this.emit(EVENTS.DETAILS_VIEW_SELECTED, viewName);
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
   * Called when a view button is clicked.
   */
  _onViewToggle: function (e) {
    this.selectView(e.target.getAttribute("data-view"));
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(DetailsView);
