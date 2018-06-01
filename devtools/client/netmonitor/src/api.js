/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { Connector } = require("./connector/index");
const { configureStore } = require("./create-store");
const { EVENTS } = require("./constants");
const Actions = require("./actions/index");

const {
  getDisplayedRequestById,
  getSortedRequests
} = require("./selectors/index");

/**
 * API object for NetMonitor panel (like a facade). This object can be
 * consumed by other panels, WebExtension API, etc.
 *
 * This object doesn't depend on the panel UI and can be created
 * and used even if the Network panel UI doesn't exist.
 */
function NetMonitorAPI() {
  EventEmitter.decorate(this);

  // Connector to the backend.
  this.connector = new Connector();

  // Configure store/state object.
  this.store = configureStore(this.connector);

  // List of listeners for `devtools.network.onRequestFinished` WebExt API
  this._requestFinishedListeners = new Set();

  // Bind event handlers
  this.onRequestAdded = this.onRequestAdded.bind(this);
  this.actions = bindActionCreators(Actions, this.store.dispatch);
}

NetMonitorAPI.prototype = {
  async connect(toolbox) {
    // Bail out if already connected.
    if (this.toolbox) {
      return;
    }

    this.toolbox = toolbox;

    // Register listener for new requests (utilized by WebExtension API).
    this.on(EVENTS.REQUEST_ADDED, this.onRequestAdded);

    // Initialize connection to the backend. Pass `this` as the owner,
    // so this object can receive all emitted events.
    const connection = {
      tabConnection: {
        tabTarget: toolbox.target,
      },
      toolbox,
      owner: this,
    };

    await this.connectBackend(this.connector, connection, this.actions,
      this.store.getState);
  },

  /**
   * Clean up (unmount from DOM, remove listeners, disconnect).
   */
  async destroy() {
    this.off(EVENTS.REQUEST_ADDED, this.onRequestAdded);

    await this.connector.disconnect();

    if (this.harExportConnector) {
      await this.harExportConnector.disconnect();
    }
  },

  /**
   * Connect to the Firefox backend by default.
   *
   * As soon as connections to different back-ends is supported
   * this function should be responsible for picking the right API.
   */
  async connectBackend(connector, connection, actions, getState) {
    // The connection might happen during Toolbox initialization
    // so make sure the target is ready.
    await connection.tabConnection.tabTarget.makeRemote();
    return connector.connectFirefox(connection, actions, getState);
  },

  // HAR

  /**
   * Support for `devtools.network.getHAR` (get collected data as HAR)
   */
  async getHar() {
    const { HarExporter } = require("devtools/client/netmonitor/src/har/har-exporter");
    const state = this.store.getState();

    const options = {
      connector: this.connector,
      items: getSortedRequests(state),
    };

    return HarExporter.getHar(options);
  },

  /**
   * Support for `devtools.network.onRequestFinished`. A hook for
   * every finished HTTP request used by WebExtensions API.
   */
  async onRequestAdded(requestId) {
    if (!this._requestFinishedListeners.size) {
      return;
    }

    const { HarExporter } = require("devtools/client/netmonitor/src/har/har-exporter");

    const connector = await this.getHarExportConnector();
    const request = getDisplayedRequestById(this.store.getState(), requestId);
    if (!request) {
      console.error("HAR: request not found " + requestId);
      return;
    }

    const options = {
      connector,
      includeResponseBodies: false,
      items: [request],
    };

    const har = await HarExporter.getHar(options);

    // There is page so remove the page reference.
    const harEntry = har.log.entries[0];
    delete harEntry.pageref;

    this._requestFinishedListeners.forEach(listener => listener({
      harEntry,
      requestId,
    }));
  },

  /**
   * Support for `Request.getContent` WebExt API (lazy loading response body)
   */
  async fetchResponseContent(requestId) {
    return this.connector.requestData(requestId, "responseContent");
  },

  /**
   * Add listener for `onRequestFinished` events.
   *
   * @param {Object} listener
   *        The listener to be called it's expected to be
   *        a function that takes ({harEntry, requestId})
   *        as first argument.
   */
  addRequestFinishedListener: function(listener) {
    this._requestFinishedListeners.add(listener);
  },

  removeRequestFinishedListener: function(listener) {
    this._requestFinishedListeners.delete(listener);
  },

  hasRequestFinishedListeners: function() {
    return this._requestFinishedListeners.size > 0;
  },

  /**
   * Separate connector for HAR export.
   */
  async getHarExportConnector() {
    if (this.harExportConnector) {
      return this.harExportConnector;
    }

    const connection = {
      tabConnection: {
        tabTarget: this.toolbox.target,
      },
      toolbox: this.toolbox,
    };

    this.harExportConnector = new Connector();
    await this.connectBackend(this.harExportConnector, connection);
    return this.harExportConnector;
  },
};

exports.NetMonitorAPI = NetMonitorAPI;
