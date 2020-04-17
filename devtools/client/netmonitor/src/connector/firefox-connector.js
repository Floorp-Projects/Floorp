/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  ACTIVITY_TYPE,
  EVENTS,
  TEST_EVENTS,
} = require("devtools/client/netmonitor/src/constants");
const FirefoxDataProvider = require("devtools/client/netmonitor/src/connector/firefox-data-provider");
const {
  getDisplayedTimingMarker,
} = require("devtools/client/netmonitor/src/selectors/index");

// Network throttling
loader.lazyRequireGetter(
  this,
  "throttlingProfiles",
  "devtools/client/shared/components/throttling/profiles"
);

/**
 * Connector to Firefox backend.
 */
class FirefoxConnector {
  constructor() {
    // Public methods
    this.connect = this.connect.bind(this);
    this.disconnect = this.disconnect.bind(this);
    this.willNavigate = this.willNavigate.bind(this);
    this.navigate = this.navigate.bind(this);
    this.displayCachedEvents = this.displayCachedEvents.bind(this);
    this.onDocEvent = this.onDocEvent.bind(this);
    this.sendHTTPRequest = this.sendHTTPRequest.bind(this);
    this.setPreferences = this.setPreferences.bind(this);
    this.triggerActivity = this.triggerActivity.bind(this);
    this.getTabTarget = this.getTabTarget.bind(this);
    this.viewSourceInDebugger = this.viewSourceInDebugger.bind(this);
    this.requestData = this.requestData.bind(this);
    this.getTimingMarker = this.getTimingMarker.bind(this);
    this.updateNetworkThrottling = this.updateNetworkThrottling.bind(this);

    // Internals
    this.getLongString = this.getLongString.bind(this);
    this.getNetworkRequest = this.getNetworkRequest.bind(this);
    this.onTargetAvailable = this.onTargetAvailable.bind(this);
  }

  get currentTarget() {
    return this.toolbox.targetList.targetFront;
  }

  /**
   * Connect to the backend.
   *
   * @param {Object} connection object with e.g. reference to the Toolbox.
   * @param {Object} actions (optional) is used to fire Redux actions to update store.
   * @param {Object} getState (optional) is used to get access to the state.
   */
  async connect(connection, actions, getState) {
    this.actions = actions;
    this.getState = getState;
    this.toolbox = connection.toolbox;

    // The owner object (NetMonitorAPI) received all events.
    this.owner = connection.owner;

    await this.toolbox.targetList.watchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable
    );
  }

  disconnect() {
    // As this function might be called twice, we need to guard if already called.
    if (this._destroyed) {
      return;
    }

    this._destroyed = true;

    this.toolbox.targetList.unwatchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable
    );

    if (this.actions) {
      this.actions.batchReset();
    }

    this.removeListeners();

    this.currentTarget.off("will-navigate", this.willNavigate);
    this.currentTarget.off("navigate", this.navigate);

    this.webConsoleFront = null;
    this.dataProvider = null;
  }

  async pause() {
    await this.removeListeners();
  }

  async resume() {
    await this.addListeners();
  }

  async onTargetAvailable({ targetFront, isTopLevel, isTargetSwitching }) {
    if (!isTopLevel) {
      return;
    }

    if (isTargetSwitching) {
      this.willNavigate();
    }

    // Listener for `will-navigate` event is (un)registered outside
    // of the `addListeners` and `removeListeners` methods since
    // these are used to pause/resume the connector.
    // Paused network panel should be automatically resumed when page
    // reload, so `will-navigate` listener needs to be there all the time.
    targetFront.on("will-navigate", this.willNavigate);
    targetFront.on("navigate", this.navigate);

    this.webConsoleFront = await this.currentTarget.getFront("console");

    this.dataProvider = new FirefoxDataProvider({
      webConsoleFront: this.webConsoleFront,
      actions: this.actions,
      owner: this.owner,
    });

    // Register all listeners
    await this.addListeners();

    // Initialize Responsive Emulation front for network throttling.
    this.responsiveFront = await this.currentTarget.getFront("responsive");

    // Displaying cache events is only intended for the UI panel.
    if (this.actions) {
      this.displayCachedEvents();
    }
  }

  async addListeners() {
    this.webConsoleFront.on("networkEvent", this.dataProvider.onNetworkEvent);
    this.webConsoleFront.on(
      "networkEventUpdate",
      this.dataProvider.onNetworkEventUpdate
    );
    this.webConsoleFront.on("documentEvent", this.onDocEvent);

    // Support for WebSocket monitoring is currently hidden behind this pref.
    if (Services.prefs.getBoolPref("devtools.netmonitor.features.webSockets")) {
      try {
        // Initialize WebSocket front to intercept websocket traffic.
        const webSocketFront = await this.currentTarget.getFront("webSocket");
        webSocketFront.startListening();

        webSocketFront.on(
          "webSocketOpened",
          this.dataProvider.onWebSocketOpened
        );
        webSocketFront.on(
          "webSocketClosed",
          this.dataProvider.onWebSocketClosed
        );
        webSocketFront.on("frameReceived", this.dataProvider.onFrameReceived);
        webSocketFront.on("frameSent", this.dataProvider.onFrameSent);
      } catch (e) {
        // Support for FF68 or older
      }
    }

    // The console actor supports listening to document events like
    // DOMContentLoaded and load.
    await this.webConsoleFront.startListeners(["DocumentEvents"]);
  }

  removeListeners() {
    const webSocketFront = this.currentTarget.getCachedFront("webSocket");
    if (webSocketFront) {
      webSocketFront.off(
        "webSocketOpened",
        this.dataProvider.onWebSocketOpened
      );
      webSocketFront.off(
        "webSocketClosed",
        this.dataProvider.onWebSocketClosed
      );
      webSocketFront.off("frameReceived", this.dataProvider.onFrameReceived);
      webSocketFront.off("frameSent", this.dataProvider.onFrameSent);
    }

    if (this.webConsoleFront) {
      this.webConsoleFront.off(
        "networkEvent",
        this.dataProvider.onNetworkEvent
      );
      this.webConsoleFront.off(
        "networkEventUpdate",
        this.dataProvider.onNetworkEventUpdate
      );
      this.webConsoleFront.off("docEvent", this.onDocEvent);
    }
  }

  enableActions(enable) {
    this.dataProvider.enableActions(enable);
  }

  willNavigate() {
    if (this.actions) {
      if (!Services.prefs.getBoolPref("devtools.netmonitor.persistlog")) {
        this.actions.batchReset();
        this.actions.clearRequests();
      } else {
        // If the log is persistent, just clear all accumulated timing markers.
        this.actions.clearTimingMarkers();
      }
    }

    if (this.actions && this.getState) {
      const state = this.getState();
      // Resume is done automatically on page reload/navigation.
      if (!state.requests.recording) {
        this.actions.toggleRecording();
      }

      // Stop any ongoing search.
      if (state.search.ongoingSearch) {
        this.actions.stopOngoingSearch();
      }
    }
  }

  navigate() {
    if (this.dataProvider.isPayloadQueueEmpty()) {
      this.onReloaded();
      return;
    }
    const listener = () => {
      if (this.dataProvider && !this.dataProvider.isPayloadQueueEmpty()) {
        return;
      }
      if (this.owner) {
        this.owner.off(EVENTS.PAYLOAD_READY, listener);
      }
      // Netmonitor may already be destroyed,
      // so do not try to notify the listeners
      if (this.dataProvider) {
        this.onReloaded();
      }
    };
    if (this.owner) {
      this.owner.on(EVENTS.PAYLOAD_READY, listener);
    }
  }

  onReloaded() {
    const panel = this.toolbox.getPanel("netmonitor");
    if (panel) {
      panel.emit("reloaded");
    }
  }

  /**
   * Display any network events already in the cache.
   */
  displayCachedEvents() {
    for (const networkInfo of this.webConsoleFront.getNetworkEvents()) {
      // First add the request to the timeline.
      this.dataProvider.onNetworkEvent(networkInfo);
      // Then replay any updates already received.
      for (const updateType of networkInfo.updates) {
        this.dataProvider.onNetworkEventUpdate({
          packet: { updateType },
          networkInfo,
        });
      }
    }
  }

  /**
   * The "DOMContentLoaded" and "Load" events sent by the console actor.
   *
   * @param {object} marker
   */
  onDocEvent(event) {
    if (this.actions) {
      this.actions.addTimingMarker(event);
    }

    this.emitForTests(TEST_EVENTS.TIMELINE_EVENT, event);
  }

  /**
   * Send a HTTP request data payload
   *
   * @param {object} data data payload would like to sent to backend
   * @param {function} callback callback will be invoked after the request finished
   */
  sendHTTPRequest(data, callback) {
    this.webConsoleFront.sendHTTPRequest(data).then(callback);
  }

  /**
   * Block future requests matching a filter.
   *
   * @param {object} filter request filter specifying what to block
   */
  blockRequest(filter) {
    return this.webConsoleFront.blockRequest(filter);
  }

  /**
   * Unblock future requests matching a filter.
   *
   * @param {object} filter request filter specifying what to unblock
   */
  unblockRequest(filter) {
    return this.webConsoleFront.unblockRequest(filter);
  }

  /**
   * Updates the list of blocked URLs
   *
   * @param {object} urls An array of URL strings
   */
  setBlockedUrls(urls) {
    return this.webConsoleFront.setBlockedUrls(urls);
  }

  /**
   * Set network preferences to control network flow
   *
   * @param {object} request request payload would like to sent to backend
   * @param {function} callback callback will be invoked after the request finished
   */
  setPreferences(request) {
    return this.webConsoleFront.setPreferences(request);
  }

  /**
   * Triggers a specific "activity" to be performed by the frontend.
   * This can be, for example, triggering reloads or enabling/disabling cache.
   *
   * @param {number} type The activity type. See the ACTIVITY_TYPE const.
   * @return {object} A promise resolved once the activity finishes and the frontend
   *                  is back into "standby" mode.
   */
  triggerActivity(type) {
    // Puts the frontend into "standby" (when there's no particular activity).
    const standBy = () => {
      this.currentActivity = ACTIVITY_TYPE.NONE;
    };

    // Waits for a series of "navigation start" and "navigation stop" events.
    const waitForNavigation = async () => {
      await this.currentTarget.once("will-navigate");
      await this.currentTarget.once("navigate");
    };

    // Reconfigures the tab, optionally triggering a reload.
    const reconfigureTab = options => {
      return this.currentTarget.reconfigure({ options });
    };

    // Reconfigures the tab and waits for the target to finish navigating.
    const reconfigureTabAndWaitForNavigation = options => {
      options.performReload = true;
      const navigationFinished = waitForNavigation();
      return reconfigureTab(options).then(() => navigationFinished);
    };
    switch (type) {
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT:
        return reconfigureTabAndWaitForNavigation({}).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED:
        this.currentActivity = ACTIVITY_TYPE.ENABLE_CACHE;
        this.currentTarget.once("will-navigate", () => {
          this.currentActivity = type;
        });
        return reconfigureTabAndWaitForNavigation({
          cacheDisabled: false,
          performReload: true,
        }).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED:
        this.currentActivity = ACTIVITY_TYPE.DISABLE_CACHE;
        this.currentTarget.once("will-navigate", () => {
          this.currentActivity = type;
        });
        return reconfigureTabAndWaitForNavigation({
          cacheDisabled: true,
          performReload: true,
        }).then(standBy);
      case ACTIVITY_TYPE.ENABLE_CACHE:
        this.currentActivity = type;
        return reconfigureTab({
          cacheDisabled: false,
          performReload: false,
        }).then(standBy);
      case ACTIVITY_TYPE.DISABLE_CACHE:
        this.currentActivity = type;
        return reconfigureTab({
          cacheDisabled: true,
          performReload: false,
        }).then(standBy);
    }
    this.currentActivity = ACTIVITY_TYPE.NONE;
    return Promise.reject(new Error("Invalid activity type"));
  }

  /**
   * Fetches the network information packet from actor server
   *
   * @param {string} id request id
   * @return {object} networkInfo data packet
   */
  getNetworkRequest(id) {
    return this.dataProvider.getNetworkRequest(id);
  }

  /**
   * Fetches the full text of a LongString.
   *
   * @param {object|string} stringGrip
   *        The long string grip containing the corresponding actor.
   *        If you pass in a plain string (by accident or because you're lazy),
   *        then a promise of the same string is simply returned.
   * @return {object}
   *         A promise that is resolved when the full string contents
   *         are available, or rejected if something goes wrong.
   */
  getLongString(stringGrip) {
    return this.dataProvider.getLongString(stringGrip);
  }

  /**
   * Getter that access tab target instance.
   * @return {object} browser tab target instance
   */
  getTabTarget() {
    return this.currentTarget;
  }

  /**
   * Open a given source in Debugger
   * @param {string} sourceURL source url
   * @param {number} sourceLine source line number
   */
  viewSourceInDebugger(sourceURL, sourceLine, sourceColumn) {
    if (this.toolbox) {
      this.toolbox.viewSourceInDebugger(sourceURL, sourceLine, sourceColumn);
    }
  }

  /**
   * Fetch networkEventUpdate websocket message from back-end when
   * data provider is connected.
   * @param {object} request network request instance
   * @param {string} type NetworkEventUpdate type
   */
  requestData(request, type) {
    return this.dataProvider.requestData(request, type);
  }

  getTimingMarker(name) {
    if (!this.getState) {
      return -1;
    }

    const state = this.getState();
    return getDisplayedTimingMarker(state, name);
  }

  async updateNetworkThrottling(enabled, profile) {
    if (!enabled) {
      await this.responsiveFront.clearNetworkThrottling();
    } else {
      const data = throttlingProfiles.find(({ id }) => id == profile);
      const { download, upload, latency } = data;
      await this.responsiveFront.setNetworkThrottling({
        downloadThroughput: download,
        uploadThroughput: upload,
        latency,
      });
    }

    this.emitForTests(TEST_EVENTS.THROTTLING_CHANGED, { profile });
  }

  /**
   * Fire events for the owner object. These events are only
   * used in tests so, don't fire them in production release.
   */
  emitForTests(type, data) {
    if (this.owner) {
      this.owner.emitForTests(type, data);
    }
  }
}

module.exports = FirefoxConnector;
