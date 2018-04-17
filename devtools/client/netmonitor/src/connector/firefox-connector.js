/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { ACTIVITY_TYPE, EVENTS } = require("../constants");
const FirefoxDataProvider = require("./firefox-data-provider");
const { getDisplayedTimingMarker } = require("../selectors/index");

// To be removed once FF60 is deprecated
loader.lazyRequireGetter(this, "TimelineFront", "devtools/shared/fronts/timeline", true);

class FirefoxConnector {
  constructor() {
    // Public methods
    this.connect = this.connect.bind(this);
    this.disconnect = this.disconnect.bind(this);
    this.willNavigate = this.willNavigate.bind(this);
    this.navigate = this.navigate.bind(this);
    this.displayCachedEvents = this.displayCachedEvents.bind(this);
    this.onDocLoadingMarker = this.onDocLoadingMarker.bind(this);
    this.onDocEvent = this.onDocEvent.bind(this);
    this.sendHTTPRequest = this.sendHTTPRequest.bind(this);
    this.setPreferences = this.setPreferences.bind(this);
    this.triggerActivity = this.triggerActivity.bind(this);
    this.getTabTarget = this.getTabTarget.bind(this);
    this.viewSourceInDebugger = this.viewSourceInDebugger.bind(this);
    this.requestData = this.requestData.bind(this);
    this.getTimingMarker = this.getTimingMarker.bind(this);

    // Internals
    this.getLongString = this.getLongString.bind(this);
    this.getNetworkRequest = this.getNetworkRequest.bind(this);
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
    this.tabTarget = connection.tabConnection.tabTarget;
    this.toolbox = connection.toolbox;

    // The owner object (NetMonitorAPI) received all events.
    this.owner = connection.owner;

    this.webConsoleClient = this.tabTarget.activeConsole;

    this.dataProvider = new FirefoxDataProvider({
      webConsoleClient: this.webConsoleClient,
      actions: this.actions,
      owner: this.owner,
    });

    await this.addListeners();

    // Listener for `will-navigate` event is (un)registered outside
    // of the `addListeners` and `removeListeners` methods since
    // these are used to pause/resume the connector.
    // Paused network panel should be automatically resumed when page
    // reload, so `will-navigate` listener needs to be there all the time.
    if (this.tabTarget) {
      this.tabTarget.on("will-navigate", this.willNavigate);
      this.tabTarget.on("navigate", this.navigate);
    }

    // Displaying cache events is only intended for the UI panel.
    if (this.actions) {
      this.displayCachedEvents();
    }
  }

  async disconnect() {
    if (this.actions) {
      this.actions.batchReset();
    }

    await this.removeListeners();

    if (this.tabTarget) {
      this.tabTarget.off("will-navigate", this.willNavigate);
      this.tabTarget.off("navigate", this.navigate);
      this.tabTarget = null;
    }

    this.webConsoleClient = null;
    this.dataProvider = null;
  }

  async pause() {
    await this.removeListeners();
  }

  async resume() {
    await this.addListeners();
  }

  async addListeners() {
    this.tabTarget.on("close", this.disconnect);
    this.webConsoleClient.on("networkEvent",
      this.dataProvider.onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate",
      this.dataProvider.onNetworkEventUpdate);
    this.webConsoleClient.on("documentEvent", this.onDocEvent);

    // With FF60+ console actor supports listening to document events like
    // DOMContentLoaded and load. We used to query Timeline actor, but it was too CPU
    // intensive.
    let { startedListeners } = await this.webConsoleClient.startListeners(
      ["DocumentEvents"]);
    // Allows to know if we are on FF60 and support these events.
    let supportsDocEvents = startedListeners.includes("DocumentEvents");

    // Don't start up waiting for timeline markers if the server isn't
    // recent enough (<FF45) to emit the markers we're interested in.
    if (!supportsDocEvents && !this.timelineFront &&
        this.tabTarget.getTrait("documentLoadingMarkers")) {
      this.timelineFront = new TimelineFront(this.tabTarget.client, this.tabTarget.form);
      this.timelineFront.on("doc-loading", this.onDocLoadingMarker);
      await this.timelineFront.start({ withDocLoadingEvents: true });
    }
  }

  async removeListeners() {
    if (this.tabTarget) {
      this.tabTarget.off("close");
    }
    if (this.timelineFront) {
      this.timelineFront.off("doc-loading", this.onDocLoadingMarker);
      await this.timelineFront.destroy();
      this.timelineFront = null;
    }
    if (this.webConsoleClient) {
      this.webConsoleClient.off("networkEvent");
      this.webConsoleClient.off("networkEventUpdate");
      this.webConsoleClient.off("docEvent");
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

    // Resume is done automatically on page reload/navigation.
    if (this.actions && this.getState) {
      let state = this.getState();
      if (!state.requests.recording) {
        this.actions.toggleRecording();
      }
    }
  }

  navigate() {
    if (this.dataProvider.isPayloadQueueEmpty()) {
      this.onReloaded();
      return;
    }
    let listener = () => {
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
    let panel = this.toolbox.getPanel("netmonitor");
    if (panel) {
      panel.emit("reloaded");
    }
  }

  /**
   * Display any network events already in the cache.
   */
  displayCachedEvents() {
    for (let networkInfo of this.webConsoleClient.getNetworkEvents()) {
      // First add the request to the timeline.
      this.dataProvider.onNetworkEvent(networkInfo);
      // Then replay any updates already received.
      for (let updateType of networkInfo.updates) {
        this.dataProvider.onNetworkEventUpdate({
          packet: { updateType },
          networkInfo,
        });
      }
    }
  }

  /**
   * The "DOMContentLoaded" and "Load" events sent by the timeline actor.
   *
   * To be removed once FF60 is deprecated.
   *
   * @param {object} marker
   */
  onDocLoadingMarker(marker) {
    // Translate marker into event similar to newer "docEvent" event sent by the console
    // actor
    let event = {
      name: marker.name == "document::DOMContentLoaded" ?
            "dom-interactive" : "dom-complete",
      time: marker.unixTime / 1000
    };

    if (this.actions) {
      this.actions.addTimingMarker(event);
    }

    this.emit(EVENTS.TIMELINE_EVENT, event);
  }

  /**
   * The "DOMContentLoaded" and "Load" events sent by the console actor.
   *
   * Only used by FF60+.
   *
   * @param {object} marker
   */
  onDocEvent(event) {
    if (this.actions) {
      this.actions.addTimingMarker(event);
    }

    this.emit(EVENTS.TIMELINE_EVENT, event);
  }

  /**
   * Send a HTTP request data payload
   *
   * @param {object} data data payload would like to sent to backend
   * @param {function} callback callback will be invoked after the request finished
   */
  sendHTTPRequest(data, callback) {
    this.webConsoleClient.sendHTTPRequest(data, callback);
  }

  /**
   * Set network preferences to control network flow
   *
   * @param {object} request request payload would like to sent to backend
   * @param {function} callback callback will be invoked after the request finished
   */
  setPreferences(request, callback) {
    this.webConsoleClient.setPreferences(request, callback);
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
    let standBy = () => {
      this.currentActivity = ACTIVITY_TYPE.NONE;
    };

    // Waits for a series of "navigation start" and "navigation stop" events.
    let waitForNavigation = () => {
      return new Promise((resolve) => {
        this.tabTarget.once("will-navigate", () => {
          this.tabTarget.once("navigate", () => {
            resolve();
          });
        });
      });
    };

    // Reconfigures the tab, optionally triggering a reload.
    let reconfigureTab = (options) => {
      return new Promise((resolve) => {
        this.tabTarget.activeTab.reconfigure(options, resolve);
      });
    };

    // Reconfigures the tab and waits for the target to finish navigating.
    let reconfigureTabAndWaitForNavigation = (options) => {
      options.performReload = true;
      let navigationFinished = waitForNavigation();
      return reconfigureTab(options).then(() => navigationFinished);
    };
    switch (type) {
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT:
        return reconfigureTabAndWaitForNavigation({}).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED:
        this.currentActivity = ACTIVITY_TYPE.ENABLE_CACHE;
        this.tabTarget.once("will-navigate", () => {
          this.currentActivity = type;
        });
        return reconfigureTabAndWaitForNavigation({
          cacheDisabled: false,
          performReload: true,
        }).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED:
        this.currentActivity = ACTIVITY_TYPE.DISABLE_CACHE;
        this.tabTarget.once("will-navigate", () => {
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
    return this.tabTarget;
  }

  /**
   * Open a given source in Debugger
   * @param {string} sourceURL source url
   * @param {number} sourceLine source line number
   */
  viewSourceInDebugger(sourceURL, sourceLine) {
    if (this.toolbox) {
      this.toolbox.viewSourceInDebugger(sourceURL, sourceLine);
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

    let state = this.getState();
    return getDisplayedTimingMarker(state, name);
  }

  /**
   * Fire events for the owner object.
   */
  emit(type, data) {
    if (this.owner) {
      this.owner.emit(type, data);
    }
  }
}

module.exports = FirefoxConnector;
