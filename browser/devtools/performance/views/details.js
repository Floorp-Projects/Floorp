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
   * Name to index mapping of subviews, used by selecting view.
   */
  components: {
    waterfall: { index: 0, view: WaterfallView },
    calltree: { index: 1, view: CallTreeView },
    flamegraph: { index: 2, view: FlameGraphView }
  },

  /**
   * Sets up the view with event binding, initializes subviews.
   */
  initialize: Task.async(function *() {
    this.el = $("#details-pane");
    this.toolbar = $("#performance-toolbar-controls-detail-views");

    this._onViewToggle = this._onViewToggle.bind(this);

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.addEventListener("command", this._onViewToggle);
    }

    yield WaterfallView.initialize();
    yield CallTreeView.initialize();
    yield FlameGraphView.initialize();

    this.selectView(DEFAULT_DETAILS_SUBVIEW);
  }),

  /**
   * Unbinds events, destroys subviews.
   */
  destroy: Task.async(function *() {
    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      button.removeEventListener("command", this._onViewToggle);
    }

    yield WaterfallView.destroy();
    yield CallTreeView.destroy();
    yield FlameGraphView.destroy();
  }),

  /**
   * Select one of the DetailView's subviews to be rendered,
   * hiding the others.
   *
   * @param String viewName
   *        Name of the view to be shown.
   */
  selectView: function (viewName) {
    this.el.selectedIndex = this.components[viewName].index;

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
    let selectedIndex = this.el.selectedIndex;

    for (let [, { index, view }] of Iterator(this.components)) {
      if (index == selectedIndex && view == viewObject) {
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
