/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */
/* globals $, gStore, NetMonitorController */

"use strict";

const { RequestsMenuView } = require("./requests-menu-view");
const { ACTIVITY_TYPE } = require("./constants");
const { createFactory } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

// Components
const NetworkDetailsPanel = createFactory(require("./shared/components/network-details-panel"));
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
    this._body = $("#body");

    this.networkDetailsPanel = $("#react-network-details-panel-hook");

    ReactDOM.render(Provider(
      { store: gStore },
      NetworkDetailsPanel({ toolbox: NetMonitorController._toolbox }),
    ), this.networkDetailsPanel);

    this.statisticsPanel = $("#react-statistics-panel-hook");

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
    this.RequestsMenu.destroy();
    ReactDOM.unmountComponentAtNode(this.networkDetailsPanel);
    ReactDOM.unmountComponentAtNode(this.statisticsPanel);
    ReactDOM.unmountComponentAtNode(this.toolbar);
    this.unsubscribeStore();
  },

  toggleFrontendMode: function () {
    if (gStore.getState().ui.statisticsOpen) {
      this._body.selectedPanel = $("#react-statistics-panel-hook");
      NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
    } else {
      this._body.selectedPanel = $("#inspector-panel");
    }
  },
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
NetMonitorView.RequestsMenu = new RequestsMenuView();

exports.NetMonitorView = NetMonitorView;
