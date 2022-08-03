/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { Connector } = require("devtools/client/netmonitor/src/connector/index");
const {
  configureStore,
} = require("devtools/client/netmonitor/src/create-store");
const { EVENTS } = require("devtools/client/netmonitor/src/constants");
const Actions = require("devtools/client/netmonitor/src/actions/index");

// Telemetry
const Telemetry = require("devtools/client/shared/telemetry");

const {
  getDisplayedRequestById,
  getSortedRequests,
} = require("devtools/client/netmonitor/src/selectors/index");

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

  // Telemetry
  this.telemetry = new Telemetry();

  // Configure store/state object.
  this.store = configureStore(this.connector, this.telemetry);

  // List of listeners for `devtools.network.onRequestFinished` WebExt API
  this._requestFinishedListeners = new Set();

  // Bind event handlers
  this.onPayloadReady = this.onPayloadReady.bind(this);
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
    this.on(EVENTS.PAYLOAD_READY, this.onPayloadReady);

    // Initialize connection to the backend. Pass `this` as the owner,
    // so this object can receive all emitted events.
    const connection = {
      toolbox,
      owner: this,
    };

    await this.connector.connect(connection, this.actions, this.store.getState);
  },

  /**
   * Clean up (unmount from DOM, remove listeners, disconnect).
   */
  destroy() {
    this.off(EVENTS.PAYLOAD_READY, this.onPayloadReady);

    this.connector.disconnect();

    if (this.harExportConnector) {
      this.harExportConnector.disconnect();
    }
  },

  // HAR

  /**
   * Support for `devtools.network.getHAR` (get collected data as HAR)
   */
  async getHar() {
    const {
      HarExporter,
    } = require("devtools/client/netmonitor/src/har/har-exporter");
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
  async onPayloadReady(resource) {
    if (!this._requestFinishedListeners.size) {
      return;
    }

    const {
      HarExporter,
    } = require("devtools/client/netmonitor/src/har/har-exporter");

    const connector = await this.getHarExportConnector();
    const request = getDisplayedRequestById(
      this.store.getState(),
      resource.actor
    );
    if (!request) {
      console.error("HAR: request not found " + resource.actor);
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

    this._requestFinishedListeners.forEach(listener =>
      listener({
        harEntry,
        requestId: resource.actor,
      })
    );
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
  addRequestFinishedListener(listener) {
    this._requestFinishedListeners.add(listener);
  },

  removeRequestFinishedListener(listener) {
    this._requestFinishedListeners.delete(listener);
  },

  hasRequestFinishedListeners() {
    return this._requestFinishedListeners.size > 0;
  },

  /**
   * Separate connector for HAR export.
   */
  async getHarExportConnector() {
    if (this.harExportConnector) {
      // Wait for the connector to be ready to avoid exceptions if this method is called
      // twice during its initialization.
      await this.harExportConnectorReady;
      return this.harExportConnector;
    }

    const connection = {
      toolbox: this.toolbox,
    };

    this.harExportConnector = new Connector();
    this.harExportConnectorReady = this.harExportConnector.connect(connection);

    await this.harExportConnectorReady;
    return this.harExportConnector;
  },

  /**
   * Resends a given network request
   * @param {String} requestId
   *        Id of the network request
   */
  resendRequest(requestId) {
    // Flush queued requests.
    this.store.dispatch(Actions.batchFlush());
    // Send custom request with same url, headers and body as the request
    // with the given requestId.
    this.store.dispatch(Actions.sendCustomRequest(this.connector, requestId));
  },
};

exports.NetMonitorAPI = NetMonitorAPI;
