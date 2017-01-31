/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals $, gStore, NetMonitorController, dumpn */

"use strict";

const { Task } = require("devtools/shared/task");
const { ViewHelpers } = require("devtools/client/shared/widgets/view-helpers");
const { RequestsMenuView } = require("./requests-menu-view");
const { CustomRequestView } = require("./custom-request-view");
const { SidebarView } = require("./sidebar-view");
const { ACTIVITY_TYPE } = require("./constants");
const { Prefs } = require("./prefs");
const { createFactory } = require("devtools/client/shared/vendor/react");
const Actions = require("./actions/index");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

// Components
const DetailsPanel = createFactory(require("./shared/components/details-panel"));
const StatisticsPanel = createFactory(require("./components/statistics-panel"));
const Toolbar = createFactory(require("./components/toolbar"));

/**
 * Object defining the network monitor view components.
 */
var NetMonitorView = {
  /**
   * Initializes the network monitor view.
   */
  initialize: function () {
    this._initializePanes();

    this.detailsPanel = $("#react-details-panel-hook");

    ReactDOM.render(Provider(
      { store: gStore },
      DetailsPanel({ toolbox: NetMonitorController._toolbox }),
    ), this.detailsPanel);

    this.statisticsPanel = $("#statistics-panel");

    ReactDOM.render(Provider(
      { store: gStore },
      StatisticsPanel(),
    ), this.statisticsPanel);

    this.toolbar = $("#react-toolbar-hook");

    ReactDOM.render(Provider(
      { store: gStore },
      Toolbar(),
    ), this.toolbar);

    this.RequestsMenu.initialize(gStore);
    this.CustomRequest.initialize();

    // Store watcher here is for observing the statisticsOpen state change.
    // It should be removed once we migrate to react and apply react/redex binding.
    this.unsubscribeStore = gStore.subscribe(storeWatcher(
      false,
      () => gStore.getState().ui.statisticsOpen,
      this.toggleFrontendMode.bind(this)
    ));
  },

  /**
   * Destroys the network monitor view.
   */
  destroy: function () {
    this._isDestroyed = true;
    this.RequestsMenu.destroy();
    this.CustomRequest.destroy();
    ReactDOM.unmountComponentAtNode(this.detailsPanel);
    ReactDOM.unmountComponentAtNode(this.statisticsPanel);
    ReactDOM.unmountComponentAtNode(this.toolbar);
    this.unsubscribeStore();

    this._destroyPanes();
  },

  /**
   * Initializes the UI for all the displayed panes.
   */
  _initializePanes: function () {
    dumpn("Initializing the NetMonitorView panes");

    this._body = $("#body");
    this._detailsPane = $("#details-pane");

    this._detailsPane.setAttribute("width", Prefs.networkDetailsWidth);
    this._detailsPane.setAttribute("height", Prefs.networkDetailsHeight);
    this.toggleDetailsPane({ visible: false });
  },

  /**
   * Destroys the UI for all the displayed panes.
   */
  _destroyPanes: Task.async(function* () {
    dumpn("Destroying the NetMonitorView panes");

    Prefs.networkDetailsWidth = this._detailsPane.getAttribute("width");
    Prefs.networkDetailsHeight = this._detailsPane.getAttribute("height");

    this._detailsPane = null;
  }),

  /**
   * Gets the visibility state of the network details pane.
   * @return boolean
   */
  get detailsPaneHidden() {
    return this._detailsPane.classList.contains("pane-collapsed");
  },

  /**
   * Sets the network details pane hidden or visible.
   *
   * @param object flags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   * @param number tabIndex [optional]
   *        The index of the intended selected tab in the details pane.
   */
  toggleDetailsPane: function (flags, tabIndex) {
    ViewHelpers.togglePane(flags, this._detailsPane);

    if (flags.visible) {
      this._body.classList.remove("pane-collapsed");
      gStore.dispatch(Actions.openSidebar(true));
    } else {
      this._body.classList.add("pane-collapsed");
      gStore.dispatch(Actions.openSidebar(false));
    }

    if (tabIndex !== undefined) {
      $("#event-details-pane").selectedIndex = tabIndex;
    }
  },

  get currentFrontendMode() {
    // The getter may be called from a timeout after the panel is destroyed.
    if (!this._body.selectedPanel) {
      return null;
    }
    return this._body.selectedPanel.id;
  },

  toggleFrontendMode: function () {
    if (gStore.getState().ui.statisticsOpen) {
      this.showNetworkStatisticsView();
    } else {
      this.showNetworkInspectorView();
    }
  },

  showNetworkInspectorView: function () {
    this._body.selectedPanel = $("#inspector-panel");
  },

  showNetworkStatisticsView: function () {
    this._body.selectedPanel = $("#statistics-panel");
    NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  },

  reloadPage: function () {
    NetMonitorController.triggerActivity(
      ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT);
  },

  _body: null,
  _detailsPane: null,
};

// A smart store watcher to notify store changes as necessary
function storeWatcher(initialValue, reduceValue, onChange) {
  let currentValue = initialValue;

  return () => {
    const newValue = reduceValue();
    if (newValue !== currentValue) {
      onChange();
      currentValue = newValue;
    }
  };
}

/**
 * Preliminary setup for the NetMonitorView object.
 */
NetMonitorView.Sidebar = new SidebarView();
NetMonitorView.RequestsMenu = new RequestsMenuView();
NetMonitorView.CustomRequest = new CustomRequestView();

exports.NetMonitorView = NetMonitorView;
