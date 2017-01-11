/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */
/* globals $, gStore, NetMonitorController, dumpn */

"use strict";

const { testing: isTesting } = require("devtools/shared/flags");
const { Task } = require("devtools/shared/task");
const { ViewHelpers } = require("devtools/client/shared/widgets/view-helpers");
const { RequestsMenuView } = require("./requests-menu-view");
const { CustomRequestView } = require("./custom-request-view");
const { ToolbarView } = require("./toolbar-view");
const { SidebarView } = require("./sidebar-view");
const { DetailsView } = require("./details-view");
const { StatisticsView } = require("./statistics-view");
const { ACTIVITY_TYPE } = require("./constants");
const Actions = require("./actions/index");
const { Prefs } = require("./prefs");

// ms
const WDA_DEFAULT_VERIFY_INTERVAL = 50;

// Use longer timeout during testing as the tests need this process to succeed
// and two seconds is quite short on slow debug builds. The timeout here should
// be at least equal to the general mochitest timeout of 45 seconds so that this
// never gets hit during testing.
// ms
const WDA_DEFAULT_GIVE_UP_TIMEOUT = isTesting ? 45000 : 2000;

/**
 * Object defining the network monitor view components.
 */
var NetMonitorView = {
  /**
   * Initializes the network monitor view.
   */
  initialize: function () {
    this._initializePanes();

    this.Toolbar.initialize(gStore);
    this.RequestsMenu.initialize(gStore);
    this.NetworkDetails.initialize(gStore);
    this.CustomRequest.initialize();
    this.Statistics.initialize(gStore);

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
    this.Toolbar.destroy();
    this.RequestsMenu.destroy();
    this.NetworkDetails.destroy();
    this.CustomRequest.destroy();
    this.Statistics.destroy();
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

  /**
   * Gets the current mode for this tool.
   * @return string (e.g, "network-inspector-view" or "network-statistics-view")
   */
  get currentFrontendMode() {
    // The getter may be called from a timeout after the panel is destroyed.
    if (!this._body.selectedPanel) {
      return null;
    }
    return this._body.selectedPanel.id;
  },

  /**
   * Toggles between the frontend view modes ("Inspector" vs. "Statistics").
   */
  toggleFrontendMode: function () {
    if (gStore.getState().ui.statisticsOpen) {
      this.showNetworkStatisticsView();
    } else {
      this.showNetworkInspectorView();
    }
  },

  /**
   * Switches to the "Inspector" frontend view mode.
   */
  showNetworkInspectorView: function () {
    this._body.selectedPanel = $("#network-inspector-view");
  },

  /**
   * Switches to the "Statistics" frontend view mode.
   */
  showNetworkStatisticsView: function () {
    this._body.selectedPanel = $("#network-statistics-view");

    let controller = NetMonitorController;
    let requestsView = this.RequestsMenu;
    let statisticsView = this.Statistics;

    Task.spawn(function* () {
      statisticsView.displayPlaceholderCharts();
      yield controller.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);

      try {
        // • The response headers and status code are required for determining
        // whether a response is "fresh" (cacheable).
        // • The response content size and request total time are necessary for
        // populating the statistics view.
        // • The response mime type is used for categorization.
        yield whenDataAvailable(requestsView.store, [
          "responseHeaders", "status", "contentSize", "mimeType", "totalTime"
        ]);
      } catch (ex) {
        // Timed out while waiting for data. Continue with what we have.
        console.error(ex);
      }

      const requests = requestsView.store.getState().requests.requests.valueSeq();
      statisticsView.createPrimedCacheChart(requests);
      statisticsView.createEmptyCacheChart(requests);
    });
  },

  reloadPage: function () {
    NetMonitorController.triggerActivity(
      ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT);
  },

  _body: null,
  _detailsPane: null,
};

/**
 * Makes sure certain properties are available on all objects in a data store.
 *
 * @param Store dataStore
 *        A Redux store for which to check the availability of properties.
 * @param array mandatoryFields
 *        A list of strings representing properties of objects in dataStore.
 * @return object
 *         A promise resolved when all objects in dataStore contain the
 *         properties defined in mandatoryFields.
 */
function whenDataAvailable(dataStore, mandatoryFields) {
  return new Promise((resolve, reject) => {
    let interval = setInterval(() => {
      const { requests } = dataStore.getState().requests;
      const allFieldsPresent = !requests.isEmpty() && requests.every(
        item => mandatoryFields.every(
          field => item.get(field) !== undefined
        )
      );

      if (allFieldsPresent) {
        clearInterval(interval);
        clearTimeout(timer);
        resolve();
      }
    }, WDA_DEFAULT_VERIFY_INTERVAL);

    let timer = setTimeout(() => {
      clearInterval(interval);
      reject(new Error("Timed out while waiting for data"));
    }, WDA_DEFAULT_GIVE_UP_TIMEOUT);
  });
}

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
NetMonitorView.Toolbar = new ToolbarView();
NetMonitorView.Sidebar = new SidebarView();
NetMonitorView.NetworkDetails = new DetailsView();
NetMonitorView.RequestsMenu = new RequestsMenuView();
NetMonitorView.CustomRequest = new CustomRequestView();
NetMonitorView.Statistics = new StatisticsView();

exports.NetMonitorView = NetMonitorView;
