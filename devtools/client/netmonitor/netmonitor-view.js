/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals gStore, NetMonitorController */

"use strict";

const { ACTIVITY_TYPE } = require("./constants");
const { createFactory } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

// Components
const NetworkDetailsPanel = createFactory(require("./shared/components/network-details-panel"));
const RequestList = createFactory(require("./components/request-list"));
const StatisticsPanel = createFactory(require("./components/statistics-panel"));
const Toolbar = createFactory(require("./components/toolbar"));

/**
 * Object defining the network monitor view components.
 */
exports.NetMonitorView = {
  /**
   * Initializes the network monitor view.
   */
  initialize: function () {
    this._body = document.querySelector("#body");

    this.networkDetailsPanel = document.querySelector(
      "#react-network-details-panel-hook");
    ReactDOM.render(Provider(
      { store: gStore },
      NetworkDetailsPanel({ toolbox: NetMonitorController._toolbox }),
    ), this.networkDetailsPanel);

    this.requestList = document.querySelector("#react-request-list-hook");
    ReactDOM.render(Provider(
      { store: gStore },
      RequestList({ toolbox: NetMonitorController._toolbox })
    ), this.requestList);

    this.statisticsPanel = document.querySelector("#react-statistics-panel-hook");
    ReactDOM.render(Provider(
      { store: gStore },
      StatisticsPanel(),
    ), this.statisticsPanel);

    this.toolbar = document.querySelector("#react-toolbar-hook");
    ReactDOM.render(Provider(
      { store: gStore },
      Toolbar(),
    ), this.toolbar);

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
    ReactDOM.unmountComponentAtNode(this.networkDetailsPanel);
    ReactDOM.unmountComponentAtNode(this.requestList);
    ReactDOM.unmountComponentAtNode(this.statisticsPanel);
    ReactDOM.unmountComponentAtNode(this.toolbar);
    this.unsubscribeStore();
  },

  toggleFrontendMode: function () {
    if (gStore.getState().ui.statisticsOpen) {
      this._body.selectedPanel = document.querySelector("#react-statistics-panel-hook");
      NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
    } else {
      this._body.selectedPanel = document.querySelector("#inspector-panel");
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
