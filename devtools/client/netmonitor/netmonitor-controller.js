/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals window, document, NetMonitorView */
"use strict";

var { utils: Cu } = Components;

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the monitored target begins and finishes navigating.
  TARGET_WILL_NAVIGATE: "NetMonitor:TargetWillNavigate",
  TARGET_DID_NAVIGATE: "NetMonitor:TargetNavigate",

  // When a network or timeline event is received.
  // See https://developer.mozilla.org/docs/Tools/Web_Console/remoting for
  // more information about what each packet is supposed to deliver.
  NETWORK_EVENT: "NetMonitor:NetworkEvent",
  TIMELINE_EVENT: "NetMonitor:TimelineEvent",

  // When a network event is added to the view
  REQUEST_ADDED: "NetMonitor:RequestAdded",

  // When request headers begin and finish receiving.
  UPDATING_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdating:RequestHeaders",
  RECEIVED_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdated:RequestHeaders",

  // When request cookies begin and finish receiving.
  UPDATING_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdating:RequestCookies",
  RECEIVED_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdated:RequestCookies",

  // When request post data begins and finishes receiving.
  UPDATING_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdating:RequestPostData",
  RECEIVED_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdated:RequestPostData",

  // When security information begins and finishes receiving.
  UPDATING_SECURITY_INFO: "NetMonitor::NetworkEventUpdating:SecurityInfo",
  RECEIVED_SECURITY_INFO: "NetMonitor::NetworkEventUpdated:SecurityInfo",

  // When response headers begin and finish receiving.
  UPDATING_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdating:ResponseHeaders",
  RECEIVED_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdated:ResponseHeaders",

  // When response cookies begin and finish receiving.
  UPDATING_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdating:ResponseCookies",
  RECEIVED_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdated:ResponseCookies",

  // When event timings begin and finish receiving.
  UPDATING_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdating:EventTimings",
  RECEIVED_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdated:EventTimings",

  // When response content begins, updates and finishes receiving.
  STARTED_RECEIVING_RESPONSE: "NetMonitor:NetworkEventUpdating:ResponseStart",
  UPDATING_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdating:ResponseContent",
  RECEIVED_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdated:ResponseContent",

  // When the request post params are displayed in the UI.
  REQUEST_POST_PARAMS_DISPLAYED: "NetMonitor:RequestPostParamsAvailable",

  // When the response body is displayed in the UI.
  RESPONSE_BODY_DISPLAYED: "NetMonitor:ResponseBodyAvailable",

  // When the html response preview is displayed in the UI.
  RESPONSE_HTML_PREVIEW_DISPLAYED: "NetMonitor:ResponseHtmlPreviewAvailable",

  // When the image response thumbnail is displayed in the UI.
  RESPONSE_IMAGE_THUMBNAIL_DISPLAYED:
    "NetMonitor:ResponseImageThumbnailAvailable",

  // When a tab is selected in the NetworkDetailsView and subsequently rendered.
  TAB_UPDATED: "NetMonitor:TabUpdated",

  // Fired when Sidebar has finished being populated.
  SIDEBAR_POPULATED: "NetMonitor:SidebarPopulated",

  // Fired when NetworkDetailsView has finished being populated.
  NETWORKDETAILSVIEW_POPULATED: "NetMonitor:NetworkDetailsViewPopulated",

  // Fired when CustomRequestView has finished being populated.
  CUSTOMREQUESTVIEW_POPULATED: "NetMonitor:CustomRequestViewPopulated",

  // Fired when charts have been displayed in the PerformanceStatisticsView.
  PLACEHOLDER_CHARTS_DISPLAYED: "NetMonitor:PlaceholderChartsDisplayed",
  PRIMED_CACHE_CHART_DISPLAYED: "NetMonitor:PrimedChartsDisplayed",
  EMPTY_CACHE_CHART_DISPLAYED: "NetMonitor:EmptyChartsDisplayed",

  // Fired once the NetMonitorController establishes a connection to the debug
  // target.
  CONNECTED: "connected",
};

// Descriptions for what this frontend is currently doing.
const ACTIVITY_TYPE = {
  // Standing by and handling requests normally.
  NONE: 0,

  // Forcing the target to reload with cache enabled or disabled.
  RELOAD: {
    WITH_CACHE_ENABLED: 1,
    WITH_CACHE_DISABLED: 2,
    WITH_CACHE_DEFAULT: 3
  },

  // Enabling or disabling the cache without triggering a reload.
  ENABLE_CACHE: 3,
  DISABLE_CACHE: 4
};

Cu.import("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesView.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesViewController.jsm");

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const promise = require("promise");
const Services = require("Services");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const EventEmitter = require("devtools/shared/event-emitter");
const Editor = require("devtools/client/sourceeditor/editor");
const {TimelineFront} = require("devtools/shared/fronts/timeline");
const { Task } = require("devtools/shared/task");

XPCOMUtils.defineConstant(this, "EVENTS", EVENTS);
XPCOMUtils.defineConstant(this, "ACTIVITY_TYPE", ACTIVITY_TYPE);
XPCOMUtils.defineConstant(this, "Editor", Editor);

XPCOMUtils.defineLazyModuleGetter(this, "Chart",
  "resource://devtools/client/shared/widgets/Chart.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Curl",
  "resource://devtools/client/shared/Curl.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CurlUtils",
  "resource://devtools/client/shared/Curl.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1", "nsIClipboardHelper");

XPCOMUtils.defineLazyServiceGetter(this, "DOMParser",
  "@mozilla.org/xmlextras/domparser;1", "nsIDOMParser");

Object.defineProperty(this, "NetworkHelper", {
  get: function () {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  enumerable: true
});

/**
 * Object defining the network monitor controller components.
 */
var NetMonitorController = {
  /**
   * Initializes the view and connects the monitor client.
   *
   * @return object
   *         A promise that is resolved when the monitor finishes startup.
   */
  startupNetMonitor: Task.async(function* () {
    if (this._startup) {
      return this._startup.promise;
    }
    this._startup = promise.defer();
    {
      NetMonitorView.initialize();
      yield this.connect();
    }
    this._startup.resolve();
    return undefined;
  }),

  /**
   * Destroys the view and disconnects the monitor client from the server.
   *
   * @return object
   *         A promise that is resolved when the monitor finishes shutdown.
   */
  shutdownNetMonitor: Task.async(function* () {
    if (this._shutdown) {
      return this._shutdown.promise;
    }
    this._shutdown = promise.defer();
    {
      NetMonitorView.destroy();
      this.TargetEventsHandler.disconnect();
      this.NetworkEventsHandler.disconnect();
      yield this.disconnect();
    }
    this._shutdown.resolve();
    return undefined;
  }),

  /**
   * Initiates remote or chrome network monitoring based on the current target,
   * wiring event handlers as necessary. Since the TabTarget will have already
   * started listening to network requests by now, this is largely
   * netmonitor-specific initialization.
   *
   * @return object
   *         A promise that is resolved when the monitor finishes connecting.
   */
  connect: Task.async(function* () {
    if (this._connection) {
      return this._connection.promise;
    }
    this._connection = promise.defer();

    // Some actors like AddonActor or RootActor for chrome debugging
    // aren't actual tabs.
    if (this._target.isTabActor) {
      this.tabClient = this._target.activeTab;
    }

    let connectTimeline = () => {
      // Don't start up waiting for timeline markers if the server isn't
      // recent enough to emit the markers we're interested in.
      if (this._target.getTrait("documentLoadingMarkers")) {
        this.timelineFront = new TimelineFront(this._target.client,
          this._target.form);
        return this.timelineFront.start({ withDocLoadingEvents: true });
      }
      return undefined;
    };

    this.webConsoleClient = this._target.activeConsole;
    yield connectTimeline();

    this.TargetEventsHandler.connect();
    this.NetworkEventsHandler.connect();

    window.emit(EVENTS.CONNECTED);

    this._connection.resolve();
    this._connected = true;
    return undefined;
  }),

  /**
   * Disconnects the debugger client and removes event handlers as necessary.
   */
  disconnect: Task.async(function* () {
    if (this._disconnection) {
      return this._disconnection.promise;
    }
    this._disconnection = promise.defer();

    // Wait for the connection to finish first.
    if (!this.isConnected()) {
      yield this._connection.promise;
    }

    // When debugging local or a remote instance, the connection is closed by
    // the RemoteTarget. The webconsole actor is stopped on disconnect.
    this.tabClient = null;
    this.webConsoleClient = null;

    // The timeline front wasn't initialized and started if the server wasn't
    // recent enough to emit the markers we were interested in.
    if (this._target.getTrait("documentLoadingMarkers")) {
      yield this.timelineFront.destroy();
      this.timelineFront = null;
    }

    this._disconnection.resolve();
    this._connected = false;
    return undefined;
  }),

  /**
   * Checks whether the netmonitor connection is active.
   * @return boolean
   */
  isConnected: function () {
    return !!this._connected;
  },

  /**
   * Gets the activity currently performed by the frontend.
   * @return number
   */
  getCurrentActivity: function () {
    return this._currentActivity || ACTIVITY_TYPE.NONE;
  },

  /**
   * Triggers a specific "activity" to be performed by the frontend.
   * This can be, for example, triggering reloads or enabling/disabling cache.
   *
   * @param number type
   *        The activity type. See the ACTIVITY_TYPE const.
   * @return object
   *         A promise resolved once the activity finishes and the frontend
   *         is back into "standby" mode.
   */
  triggerActivity: function (type) {
    // Puts the frontend into "standby" (when there's no particular activity).
    let standBy = () => {
      this._currentActivity = ACTIVITY_TYPE.NONE;
    };

    // Waits for a series of "navigation start" and "navigation stop" events.
    let waitForNavigation = () => {
      let deferred = promise.defer();
      this._target.once("will-navigate", () => {
        this._target.once("navigate", () => {
          deferred.resolve();
        });
      });
      return deferred.promise;
    };

    // Reconfigures the tab, optionally triggering a reload.
    let reconfigureTab = options => {
      let deferred = promise.defer();
      this._target.activeTab.reconfigure(options, deferred.resolve);
      return deferred.promise;
    };

    // Reconfigures the tab and waits for the target to finish navigating.
    let reconfigureTabAndWaitForNavigation = options => {
      options.performReload = true;
      let navigationFinished = waitForNavigation();
      return reconfigureTab(options).then(() => navigationFinished);
    };
    if (type == ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT) {
      return reconfigureTabAndWaitForNavigation({}).then(standBy);
    }
    if (type == ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED) {
      this._currentActivity = ACTIVITY_TYPE.ENABLE_CACHE;
      this._target.once("will-navigate", () => {
        this._currentActivity = type;
      });
      return reconfigureTabAndWaitForNavigation({
        cacheDisabled: false,
        performReload: true
      }).then(standBy);
    }
    if (type == ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED) {
      this._currentActivity = ACTIVITY_TYPE.DISABLE_CACHE;
      this._target.once("will-navigate", () => {
        this._currentActivity = type;
      });
      return reconfigureTabAndWaitForNavigation({
        cacheDisabled: true,
        performReload: true
      }).then(standBy);
    }
    if (type == ACTIVITY_TYPE.ENABLE_CACHE) {
      this._currentActivity = type;
      return reconfigureTab({
        cacheDisabled: false,
        performReload: false
      }).then(standBy);
    }
    if (type == ACTIVITY_TYPE.DISABLE_CACHE) {
      this._currentActivity = type;
      return reconfigureTab({
        cacheDisabled: true,
        performReload: false
      }).then(standBy);
    }
    this._currentActivity = ACTIVITY_TYPE.NONE;
    return promise.reject(new Error("Invalid activity type"));
  },

  /**
   * Selects the specified request in the waterfall and opens the details view.
   *
   * @param string requestId
   *        The actor ID of the request to inspect.
   * @return object
   *         A promise resolved once the task finishes.
   */
  inspectRequest: function (requestId) {
    // Look for the request in the existing ones or wait for it to appear, if
    // the network monitor is still loading.
    let deferred = promise.defer();
    let request = null;
    let inspector = function () {
      let predicate = i => i.value === requestId;
      request = NetMonitorView.RequestsMenu.getItemForPredicate(predicate);
      if (!request) {
        // Reset filters so that the request is visible.
        NetMonitorView.RequestsMenu.filterOn("all");
        request = NetMonitorView.RequestsMenu.getItemForPredicate(predicate);
      }

      // If the request was found, select it. Otherwise this function will be
      // called again once new requests arrive.
      if (request) {
        window.off(EVENTS.REQUEST_ADDED, inspector);
        NetMonitorView.RequestsMenu.selectedItem = request;
        deferred.resolve();
      }
    };

    inspector();
    if (!request) {
      window.on(EVENTS.REQUEST_ADDED, inspector);
    }
    return deferred.promise;
  },

  /**
   * Getter that tells if the server supports sending custom network requests.
   * @type boolean
   */
  get supportsCustomRequest() {
    return this.webConsoleClient &&
           (this.webConsoleClient.traits.customNetworkRequest ||
            !this._target.isApp);
  },

  /**
   * Getter that tells if the server includes the transferred (compressed /
   * encoded) response size.
   * @type boolean
   */
  get supportsTransferredResponseSize() {
    return this.webConsoleClient &&
           this.webConsoleClient.traits.transferredResponseSize;
  },

  /**
   * Getter that tells if the server can do network performance statistics.
   * @type boolean
   */
  get supportsPerfStats() {
    return this.tabClient &&
           (this.tabClient.traits.reconfigure || !this._target.isApp);
  },

  /**
   * Open a given source in Debugger
   */
  viewSourceInDebugger(sourceURL, sourceLine) {
    return this._toolbox.viewSourceInDebugger(sourceURL, sourceLine);
  }
};

/**
 * Functions handling target-related lifetime events.
 */
function TargetEventsHandler() {
  this._onTabNavigated = this._onTabNavigated.bind(this);
  this._onTabDetached = this._onTabDetached.bind(this);
}

TargetEventsHandler.prototype = {
  get target() {
    return NetMonitorController._target;
  },

  /**
   * Listen for events emitted by the current tab target.
   */
  connect: function () {
    dumpn("TargetEventsHandler is connecting...");
    this.target.on("close", this._onTabDetached);
    this.target.on("navigate", this._onTabNavigated);
    this.target.on("will-navigate", this._onTabNavigated);
  },

  /**
   * Remove events emitted by the current tab target.
   */
  disconnect: function () {
    if (!this.target) {
      return;
    }
    dumpn("TargetEventsHandler is disconnecting...");
    this.target.off("close", this._onTabDetached);
    this.target.off("navigate", this._onTabNavigated);
    this.target.off("will-navigate", this._onTabNavigated);
  },

  /**
   * Called for each location change in the monitored tab.
   *
   * @param string type
   *        Packet type.
   * @param object packet
   *        Packet received from the server.
   */
  _onTabNavigated: function (type, packet) {
    switch (type) {
      case "will-navigate": {
        // Reset UI.
        if (!Services.prefs.getBoolPref("devtools.webconsole.persistlog")) {
          NetMonitorView.RequestsMenu.reset();
          NetMonitorView.Sidebar.toggle(false);
        }
        // Switch to the default network traffic inspector view.
        if (NetMonitorController.getCurrentActivity() == ACTIVITY_TYPE.NONE) {
          NetMonitorView.showNetworkInspectorView();
        }
        // Clear any accumulated markers.
        NetMonitorController.NetworkEventsHandler.clearMarkers();

        window.emit(EVENTS.TARGET_WILL_NAVIGATE);
        break;
      }
      case "navigate": {
        window.emit(EVENTS.TARGET_DID_NAVIGATE);
        break;
      }
    }
  },

  /**
   * Called when the monitored tab is closed.
   */
  _onTabDetached: function () {
    NetMonitorController.shutdownNetMonitor();
  }
};

/**
 * Functions handling target network events.
 */
function NetworkEventsHandler() {
  this._markers = [];

  this._onNetworkEvent = this._onNetworkEvent.bind(this);
  this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
  this._onDocLoadingMarker = this._onDocLoadingMarker.bind(this);
  this._onRequestHeaders = this._onRequestHeaders.bind(this);
  this._onRequestCookies = this._onRequestCookies.bind(this);
  this._onRequestPostData = this._onRequestPostData.bind(this);
  this._onResponseHeaders = this._onResponseHeaders.bind(this);
  this._onResponseCookies = this._onResponseCookies.bind(this);
  this._onResponseContent = this._onResponseContent.bind(this);
  this._onEventTimings = this._onEventTimings.bind(this);
}

NetworkEventsHandler.prototype = {
  get client() {
    return NetMonitorController._target.client;
  },

  get webConsoleClient() {
    return NetMonitorController.webConsoleClient;
  },

  get timelineFront() {
    return NetMonitorController.timelineFront;
  },

  get firstDocumentDOMContentLoadedTimestamp() {
    let marker = this._markers.filter(e => {
      return e.name == "document::DOMContentLoaded";
    })[0];

    return marker ? marker.unixTime / 1000 : -1;
  },

  get firstDocumentLoadTimestamp() {
    let marker = this._markers.filter(e => e.name == "document::Load")[0];
    return marker ? marker.unixTime / 1000 : -1;
  },

  /**
   * Connect to the current target client.
   */
  connect: function () {
    dumpn("NetworkEventsHandler is connecting...");
    this.webConsoleClient.on("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this._onNetworkEventUpdate);

    if (this.timelineFront) {
      this.timelineFront.on("doc-loading", this._onDocLoadingMarker);
    }

    this._displayCachedEvents();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function () {
    if (!this.client) {
      return;
    }
    dumpn("NetworkEventsHandler is disconnecting...");
    this.webConsoleClient.off("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.off("networkEventUpdate", this._onNetworkEventUpdate);

    if (this.timelineFront) {
      this.timelineFront.off("doc-loading", this._onDocLoadingMarker);
    }
  },

  /**
   * Display any network events already in the cache.
   */
  _displayCachedEvents: function () {
    for (let cachedEvent of this.webConsoleClient.getNetworkEvents()) {
      // First add the request to the timeline.
      this._onNetworkEvent("networkEvent", cachedEvent);
      // Then replay any updates already received.
      for (let update of cachedEvent.updates) {
        this._onNetworkEventUpdate("networkEventUpdate", {
          packet: {
            updateType: update
          },
          networkInfo: cachedEvent
        });
      }
    }
  },

  /**
   * The "DOMContentLoaded" and "Load" events sent by the timeline actor.
   * @param object marker
   */
  _onDocLoadingMarker: function (marker) {
    window.emit(EVENTS.TIMELINE_EVENT, marker);
    this._markers.push(marker);
  },

  /**
   * The "networkEvent" message type handler.
   *
   * @param string type
   *        Message type.
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEvent: function (type, networkInfo) {
    let { actor,
      startedDateTime,
      request: { method, url },
      isXHR,
      cause,
      fromCache,
      fromServiceWorker
    } = networkInfo;

    NetMonitorView.RequestsMenu.addRequest(
      actor, startedDateTime, method, url, isXHR, cause, fromCache,
        fromServiceWorker
    );
    window.emit(EVENTS.NETWORK_EVENT, actor);
  },

  /**
   * The "networkEventUpdate" message type handler.
   *
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEventUpdate: function (type, { packet, networkInfo }) {
    let { actor } = networkInfo;

    switch (packet.updateType) {
      case "requestHeaders":
        this.webConsoleClient.getRequestHeaders(actor, this._onRequestHeaders);
        window.emit(EVENTS.UPDATING_REQUEST_HEADERS, actor);
        break;
      case "requestCookies":
        this.webConsoleClient.getRequestCookies(actor, this._onRequestCookies);
        window.emit(EVENTS.UPDATING_REQUEST_COOKIES, actor);
        break;
      case "requestPostData":
        this.webConsoleClient.getRequestPostData(actor,
          this._onRequestPostData);
        window.emit(EVENTS.UPDATING_REQUEST_POST_DATA, actor);
        break;
      case "securityInfo":
        NetMonitorView.RequestsMenu.updateRequest(actor, {
          securityState: networkInfo.securityInfo,
        });
        this.webConsoleClient.getSecurityInfo(actor, this._onSecurityInfo);
        window.emit(EVENTS.UPDATING_SECURITY_INFO, actor);
        break;
      case "responseHeaders":
        this.webConsoleClient.getResponseHeaders(actor,
          this._onResponseHeaders);
        window.emit(EVENTS.UPDATING_RESPONSE_HEADERS, actor);
        break;
      case "responseCookies":
        this.webConsoleClient.getResponseCookies(actor,
          this._onResponseCookies);
        window.emit(EVENTS.UPDATING_RESPONSE_COOKIES, actor);
        break;
      case "responseStart":
        NetMonitorView.RequestsMenu.updateRequest(actor, {
          httpVersion: networkInfo.response.httpVersion,
          remoteAddress: networkInfo.response.remoteAddress,
          remotePort: networkInfo.response.remotePort,
          status: networkInfo.response.status,
          statusText: networkInfo.response.statusText,
          headersSize: networkInfo.response.headersSize
        });
        window.emit(EVENTS.STARTED_RECEIVING_RESPONSE, actor);
        break;
      case "responseContent":
        NetMonitorView.RequestsMenu.updateRequest(actor, {
          contentSize: networkInfo.response.bodySize,
          transferredSize: networkInfo.response.transferredSize,
          mimeType: networkInfo.response.content.mimeType
        });
        this.webConsoleClient.getResponseContent(actor,
          this._onResponseContent);
        window.emit(EVENTS.UPDATING_RESPONSE_CONTENT, actor);
        break;
      case "eventTimings":
        NetMonitorView.RequestsMenu.updateRequest(actor, {
          totalTime: networkInfo.totalTime
        });
        this.webConsoleClient.getEventTimings(actor, this._onEventTimings);
        window.emit(EVENTS.UPDATING_EVENT_TIMINGS, actor);
        break;
    }
  },

  /**
   * Handles additional information received for a "requestHeaders" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onRequestHeaders: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      requestHeaders: response
    }, () => {
      window.emit(EVENTS.RECEIVED_REQUEST_HEADERS, response.from);
    });
  },

  /**
   * Handles additional information received for a "requestCookies" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onRequestCookies: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      requestCookies: response
    }, () => {
      window.emit(EVENTS.RECEIVED_REQUEST_COOKIES, response.from);
    });
  },

  /**
   * Handles additional information received for a "requestPostData" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onRequestPostData: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      requestPostData: response
    }, () => {
      window.emit(EVENTS.RECEIVED_REQUEST_POST_DATA, response.from);
    });
  },

  /**
   * Handles additional information received for a "securityInfo" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onSecurityInfo: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      securityInfo: response.securityInfo
    }, () => {
      window.emit(EVENTS.RECEIVED_SECURITY_INFO, response.from);
    });
  },

  /**
   * Handles additional information received for a "responseHeaders" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onResponseHeaders: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      responseHeaders: response
    }, () => {
      window.emit(EVENTS.RECEIVED_RESPONSE_HEADERS, response.from);
    });
  },

  /**
   * Handles additional information received for a "responseCookies" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onResponseCookies: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      responseCookies: response
    }, () => {
      window.emit(EVENTS.RECEIVED_RESPONSE_COOKIES, response.from);
    });
  },

  /**
   * Handles additional information received for a "responseContent" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onResponseContent: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      responseContent: response
    }, () => {
      window.emit(EVENTS.RECEIVED_RESPONSE_CONTENT, response.from);
    });
  },

  /**
   * Handles additional information received for a "eventTimings" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  _onEventTimings: function (response) {
    NetMonitorView.RequestsMenu.updateRequest(response.from, {
      eventTimings: response
    }, () => {
      window.emit(EVENTS.RECEIVED_EVENT_TIMINGS, response.from);
    });
  },

  /**
   * Clears all accumulated markers.
   */
  clearMarkers: function () {
    this._markers.length = 0;
  },

  /**
   * Fetches the full text of a LongString.
   *
   * @param object | string stringGrip
   *        The long string grip containing the corresponding actor.
   *        If you pass in a plain string (by accident or because you're lazy),
   *        then a promise of the same string is simply returned.
   * @return object Promise
   *         A promise that is resolved when the full string contents
   *         are available, or rejected if something goes wrong.
   */
  getString: function (stringGrip) {
    return this.webConsoleClient.getString(stringGrip);
  }
};

/**
 * Returns true if this is document is in RTL mode.
 * @return boolean
 */
XPCOMUtils.defineLazyGetter(window, "isRTL", function () {
  return window.getComputedStyle(document.documentElement, null)
    .direction == "rtl";
});

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * Preliminary setup for the NetMonitorController object.
 */
NetMonitorController.TargetEventsHandler = new TargetEventsHandler();
NetMonitorController.NetworkEventsHandler = new NetworkEventsHandler();

/**
 * Export some properties to the global scope for easier access.
 */
Object.defineProperties(window, {
  "gNetwork": {
    get: function () {
      return NetMonitorController.NetworkEventsHandler;
    },
    configurable: true
  }
});

/**
 * Helper method for debugging.
 * @param string
 */
function dumpn(str) {
  if (wantLogging) {
    dump("NET-FRONTEND: " + str + "\n");
  }
}

var wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");
