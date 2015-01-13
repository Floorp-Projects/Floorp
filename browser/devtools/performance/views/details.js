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
  viewIndexes: {
    waterfall: 0,
    calltree: 1,
    flamegraph: 2
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

    yield CallTreeView.initialize();
    yield WaterfallView.initialize();
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

    yield CallTreeView.destroy();
    yield WaterfallView.destroy();
    yield FlameGraphView.destroy();
  }),

  /**
   * Select one of the DetailView's subviews to be rendered,
   * hiding the others.
   *
   * @params {String} selectedView
   *         Name of the view to be shown.
   */
  selectView: function (selectedView) {
    this.el.selectedIndex = this.viewIndexes[selectedView];

    for (let button of $$("toolbarbutton[data-view]", this.toolbar)) {
      if (button.getAttribute("data-view") === selectedView) {
        button.setAttribute("checked", true);
      } else {
        button.removeAttribute("checked");
      }
    }

    this.emit(EVENTS.DETAILS_VIEW_SELECTED, selectedView);
  },

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
