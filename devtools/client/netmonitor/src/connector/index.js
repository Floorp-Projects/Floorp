/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ACTIVITY_TYPE,
  EVENTS,
  TEST_EVENTS,
} = require("devtools/client/netmonitor/src/constants");
const FirefoxDataProvider = require("devtools/client/netmonitor/src/connector/firefox-data-provider");
const {
  getDisplayedTimingMarker,
} = require("devtools/client/netmonitor/src/selectors/index");

const { TYPES } = require("devtools/shared/commands/resource/resource-command");

// Network throttling
loader.lazyRequireGetter(
  this,
  "throttlingProfiles",
  "devtools/client/shared/components/throttling/profiles"
);

const DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF = "devtools.netmonitor.persistlog";

/**
 * Connector to Firefox backend.
 */
class Connector {
  constructor() {
    // Public methods
    this.connect = this.connect.bind(this);
    this.disconnect = this.disconnect.bind(this);
    this.willNavigate = this.willNavigate.bind(this);
    this.navigate = this.navigate.bind(this);
    this.sendHTTPRequest = this.sendHTTPRequest.bind(this);
    this.triggerActivity = this.triggerActivity.bind(this);
    this.getTabTarget = this.getTabTarget.bind(this);
    this.viewSourceInDebugger = this.viewSourceInDebugger.bind(this);
    this.requestData = this.requestData.bind(this);
    this.getTimingMarker = this.getTimingMarker.bind(this);
    this.updateNetworkThrottling = this.updateNetworkThrottling.bind(this);

    // Internals
    this.getLongString = this.getLongString.bind(this);
    this.onTargetAvailable = this.onTargetAvailable.bind(this);
    this.onResourceAvailable = this.onResourceAvailable.bind(this);
    this.onResourceUpdated = this.onResourceUpdated.bind(this);
    this.updatePersist = this.updatePersist.bind(this);

    this.networkFront = null;
  }

  static NETWORK_RESOURCES = [
    TYPES.NETWORK_EVENT,
    TYPES.NETWORK_EVENT_STACKTRACE,
    TYPES.WEBSOCKET,
    TYPES.SERVER_SENT_EVENT,
  ];

  get currentTarget() {
    return this.commands.targetCommand.targetFront;
  }

  get hasResourceCommandSupport() {
    return this.toolbox.resourceCommand.hasResourceCommandSupport(
      this.toolbox.resourceCommand.TYPES.NETWORK_EVENT
    );
  }

  get watcherFront() {
    return this.toolbox.resourceCommand.watcherFront;
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
    this.commands = this.toolbox.commands;

    // The owner object (NetMonitorAPI) received all events.
    this.owner = connection.owner;

    if (this.hasResourceCommandSupport) {
      this.networkFront = await this.watcherFront.getNetworkParentActor();
    }

    this.dataProvider = new FirefoxDataProvider({
      commands: this.commands,
      actions: this.actions,
      owner: this.owner,
    });

    await this.commands.targetCommand.watchTargets({
      types: [this.commands.targetCommand.TYPES.FRAME],
      onAvailable: this.onTargetAvailable,
    });

    await this.toolbox.resourceCommand.watchResources([TYPES.DOCUMENT_EVENT], {
      onAvailable: this.onResourceAvailable,
    });

    await this.resume(false);

    // Server side persistance of the data across reload is disabled by default.
    // Ensure enabling it, if the related frontend pref is true.
    if (Services.prefs.getBoolPref(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF)) {
      await this.updatePersist();
    }
    Services.prefs.addObserver(
      DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
      this.updatePersist
    );
  }

  disconnect() {
    // As this function might be called twice, we need to guard if already called.
    if (this._destroyed) {
      return;
    }

    this._destroyed = true;

    this.commands.targetCommand.unwatchTargets({
      types: [this.commands.targetCommand.TYPES.FRAME],
      onAvailable: this.onTargetAvailable,
    });

    this.toolbox.resourceCommand.unwatchResources([TYPES.DOCUMENT_EVENT], {
      onAvailable: this.onResourceAvailable,
    });

    this.pause();

    Services.prefs.removeObserver(
      DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
      this.updatePersist
    );

    if (this.actions) {
      this.actions.batchReset();
    }

    this.webConsoleFront = null;

    this.dataProvider.destroy();
    this.dataProvider = null;
  }

  clear() {
    // Clear all the caches in the data provider
    this.dataProvider.clear();

    this.toolbox.resourceCommand.clearResources(Connector.NETWORK_RESOURCES);
    this.emitForTests("clear-network-resources");

    // Disable the realted network logs in the webconsole
    this.toolbox.disableAllConsoleNetworkLogs();
  }

  pause() {
    return this.toolbox.resourceCommand.unwatchResources(
      Connector.NETWORK_RESOURCES,
      {
        onAvailable: this.onResourceAvailable,
        onUpdated: this.onResourceUpdated,
      }
    );
  }

  resume(ignoreExistingResources = true) {
    return this.toolbox.resourceCommand.watchResources(
      Connector.NETWORK_RESOURCES,
      {
        onAvailable: this.onResourceAvailable,
        onUpdated: this.onResourceUpdated,
        ignoreExistingResources,
      }
    );
  }

  async onTargetAvailable({ targetFront, isTargetSwitching }) {
    if (!targetFront.isTopLevel) {
      return;
    }

    this.webConsoleFront = await this.currentTarget.getFront("console");

    // Initialize Responsive Emulation front for network throttling,
    // only for toolboxes using Watcher and non-legacy Resources.
    if (!this.hasResourceCommandSupport) {
      this.responsiveFront = await this.currentTarget.getFront("responsive");
    }
  }

  async onResourceAvailable(resources, { areExistingResources }) {
    for (const resource of resources) {
      if (resource.resourceType === TYPES.DOCUMENT_EVENT) {
        this.onDocEvent(resource, { areExistingResources });
        continue;
      }

      if (resource.resourceType === TYPES.NETWORK_EVENT) {
        this.dataProvider.onNetworkResourceAvailable(resource);
        continue;
      }

      if (resource.resourceType === TYPES.NETWORK_EVENT_STACKTRACE) {
        this.dataProvider.onStackTraceAvailable(resource);
        continue;
      }

      if (resource.resourceType === TYPES.WEBSOCKET) {
        const { wsMessageType } = resource;

        switch (wsMessageType) {
          case "webSocketOpened": {
            this.dataProvider.onWebSocketOpened(
              resource.httpChannelId,
              resource.effectiveURI,
              resource.protocols,
              resource.extensions
            );
            break;
          }
          case "webSocketClosed": {
            this.dataProvider.onWebSocketClosed(
              resource.httpChannelId,
              resource.wasClean,
              resource.code,
              resource.reason
            );
            break;
          }
          case "frameReceived": {
            this.dataProvider.onFrameReceived(
              resource.httpChannelId,
              resource.data
            );
            break;
          }
          case "frameSent": {
            this.dataProvider.onFrameSent(
              resource.httpChannelId,
              resource.data
            );
            break;
          }
        }
        continue;
      }

      if (resource.resourceType === TYPES.SERVER_SENT_EVENT) {
        const { messageType, httpChannelId, data } = resource;
        switch (messageType) {
          case "eventSourceConnectionClosed": {
            this.dataProvider.onEventSourceConnectionClosed(httpChannelId);
            break;
          }
          case "eventReceived": {
            this.dataProvider.onEventReceived(httpChannelId, data);
            break;
          }
        }
      }
    }
  }

  async onResourceUpdated(updates) {
    for (const { resource, update } of updates) {
      this.dataProvider.onNetworkResourceUpdated(resource, update);
    }
  }

  enableActions(enable) {
    this.dataProvider.enableActions(enable);
  }

  willNavigate() {
    if (this.actions) {
      if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF)) {
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
    if (!this.dataProvider.hasPendingRequests()) {
      this.onReloaded();
      return;
    }
    const listener = () => {
      if (this.dataProvider && this.dataProvider.hasPendingRequests()) {
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
   * The "DOMContentLoaded" and "Load" events sent by the console actor.
   *
   * @param {object} resource The DOCUMENT_EVENT resource
   */
  onDocEvent(resource, { areExistingResources }) {
    if (!resource.targetFront.isTopLevel) {
      // Only consider top level document, and ignore remote iframes top document
      return;
    }

    // Netmonitor does not support dom-loading
    if (
      resource.name != "dom-interactive" &&
      resource.name != "dom-complete" &&
      resource.name != "will-navigate"
    ) {
      return;
    }

    if (resource.name == "will-navigate") {
      // When we open the netmonitor while the page already started loading,
      // we don't want to clear it. So here, we ignore will-navigate events
      // which were stored in the ResourceCommand cache and only consider
      // the live one coming straight from the server.
      if (!areExistingResources) {
        this.willNavigate();
      }
      return;
    }

    if (this.actions) {
      this.actions.addTimingMarker(resource);
    }

    if (resource.name === "dom-complete") {
      this.navigate();
    }

    this.emitForTests(TEST_EVENTS.TIMELINE_EVENT, resource);
  }

  /**
   * Send a HTTP request data payload
   *
   * @param {object} data data payload would like to sent to backend
   */
  async sendHTTPRequest(data) {
    if (this.hasResourceCommandSupport && this.currentTarget) {
      const networkContentFront = await this.currentTarget.getFront(
        "networkContent"
      );
      const { channelId } = await networkContentFront.sendHTTPRequest(data);
      return { channelId };
    }
    const {
      eventActor: { actor },
    } = await this.webConsoleFront.sendHTTPRequest(data);
    return { actor };
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

  /*
   * Get the list of blocked URLs
   */
  async getBlockedUrls() {
    if (this.hasResourceCommandSupport && this.networkFront) {
      return this.networkFront.getBlockedUrls();
    }
    return this.webConsoleFront.getBlockedUrls();
  }

  async updatePersist() {
    const enabled = Services.prefs.getBoolPref(
      DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF
    );

    // Lets keep these checks until we stop using legacy listeners entirely. (bug 1689459)
    const hasServerSupport = this.commands.targetCommand.hasTargetWatcherSupport();
    if (
      hasServerSupport &&
      this.hasResourceCommandSupport &&
      this.networkFront
    ) {
      await this.networkFront.setPersist(enabled);
    }

    this.emitForTests(TEST_EVENTS.PERSIST_CHANGED, enabled);
  }

  /**
   * Updates the list of blocked URLs
   *
   * @param {object} urls An array of URL strings
   */
  async setBlockedUrls(urls) {
    if (this.hasResourceCommandSupport && this.networkFront) {
      return this.networkFront.setBlockedUrls(urls);
    }
    return this.webConsoleFront.setBlockedUrls(urls);
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

    // Reconfigures the tab, optionally triggering a reload.
    const reconfigureTab = async options => {
      await this.commands.targetConfigurationCommand.updateConfiguration(
        options
      );
    };

    // Reconfigures the tab and waits for the target to finish navigating.
    const reconfigureTabAndReload = async options => {
      await reconfigureTab(options);
      await this.commands.targetCommand.reloadTopLevelTarget();
    };

    switch (type) {
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT:
        return reconfigureTabAndReload({}).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED:
        this.currentActivity = ACTIVITY_TYPE.ENABLE_CACHE;
        this.commands.resourceCommand
          .waitForNextResource(
            this.commands.resourceCommand.TYPES.DOCUMENT_EVENT,
            {
              ignoreExistingResources: true,
              predicate(resource) {
                return resource.name == "will-navigate";
              },
            }
          )
          .then(() => {
            this.currentActivity = type;
          });
        return reconfigureTabAndReload({
          cacheDisabled: false,
        }).then(standBy);
      case ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED:
        this.currentActivity = ACTIVITY_TYPE.DISABLE_CACHE;
        this.commands.resourceCommand
          .waitForNextResource(
            this.commands.resourceCommand.TYPES.DOCUMENT_EVENT,
            {
              ignoreExistingResources: true,
              predicate(resource) {
                return resource.name == "will-navigate";
              },
            }
          )
          .then(() => {
            this.currentActivity = type;
          });
        return reconfigureTabAndReload({
          cacheDisabled: true,
        }).then(standBy);
      case ACTIVITY_TYPE.ENABLE_CACHE:
        this.currentActivity = type;
        return reconfigureTab({
          cacheDisabled: false,
        }).then(standBy);
      case ACTIVITY_TYPE.DISABLE_CACHE:
        this.currentActivity = type;
        return reconfigureTab({
          cacheDisabled: true,
        }).then(standBy);
    }
    this.currentActivity = ACTIVITY_TYPE.NONE;
    return Promise.reject(new Error("Invalid activity type"));
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
   * Getter that returns the current toolbox instance.
   * @return {Toolbox} toolbox instance
   */
  getToolbox() {
    return this.toolbox;
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
    const throttlingFront =
      this.hasResourceCommandSupport && this.networkFront
        ? this.networkFront
        : this.responsiveFront;

    if (!enabled) {
      throttlingFront.clearNetworkThrottling();
    } else {
      // The profile can be either a profile id which is used to
      // search the predefined throttle profiles or a profile object
      // as defined in the trottle tests.
      if (typeof profile === "string") {
        profile = throttlingProfiles.find(({ id }) => id == profile);
      }
      const { download, upload, latency } = profile;
      await throttlingFront.setNetworkThrottling({
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
module.exports.Connector = Connector;
