/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TimelineFront } = require("devtools/shared/fronts/timeline");
const { CurlUtils } = require("devtools/client/shared/curl");
const { ACTIVITY_TYPE, EVENTS } = require("./constants");
const Actions = require("./actions/index");
const {
  fetchHeaders,
  formDataURI,
} = require("./utils/request-utils");
const {
  getWebConsoleClient,
  onFirefoxConnect,
  onFirefoxDisconnect,
} = require("./utils/client");
const {
  getRequestById,
  getDisplayedRequestById,
} = require("./selectors/index");

const gStore = window.gStore;

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
  startupNetMonitor() {
    if (this._startup) {
      return this._startup;
    }
    this._startup = new Promise(async (resolve) => {
      await this.connect();
      resolve();
    });
    return this._startup;
  },

  /**
   * Destroys the view and disconnects the monitor client from the server.
   *
   * @return object
   *         A promise that is resolved when the monitor finishes shutdown.
   */
  shutdownNetMonitor() {
    if (this._shutdown) {
      return this._shutdown;
    }
    this._shutdown = new Promise(async (resolve) => {
      gStore.dispatch(Actions.batchReset());
      onFirefoxDisconnect(this._target);
      this._target.off("close", this._onTabDetached);
      this.NetworkEventsHandler.disconnect();
      await this.disconnect();
      resolve();
    });

    return this._shutdown;
  },

  /**
   * Initiates remote or chrome network monitoring based on the current target,
   * wiring event handlers as necessary. Since the TabTarget will have already
   * started listening to network requests by now, this is largely
   * netmonitor-specific initialization.
   *
   * @return object
   *         A promise that is resolved when the monitor finishes connecting.
   */
  connect() {
    if (this._connection) {
      return this._connection;
    }
    this._onTabDetached = this.shutdownNetMonitor.bind(this);

    this._connection = new Promise(async (resolve) => {
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
      await connectTimeline();

      onFirefoxConnect(this._target);
      this._target.on("close", this._onTabDetached);

      this.webConsoleClient = getWebConsoleClient();
      this.NetworkEventsHandler = new NetworkEventsHandler();
      this.NetworkEventsHandler.connect();

      window.gNetwork = this.NetworkEventsHandler;
      window.emit(EVENTS.CONNECTED);

      resolve();
      this._connected = true;
    });
    return this._connection;
  },

  /**
   * Disconnects the debugger client and removes event handlers as necessary.
   */
  disconnect() {
    if (this._disconnection) {
      return this._disconnection;
    }
    this._disconnection = new Promise(async (resolve) => {
      // Wait for the connection to finish first.
      if (!this.isConnected()) {
        await this._connection;
      }

      // When debugging local or a remote instance, the connection is closed by
      // the RemoteTarget. The webconsole actor is stopped on disconnect.
      this.tabClient = null;
      this.webConsoleClient = null;

      // The timeline front wasn't initialized and started if the server wasn't
      // recent enough to emit the markers we were interested in.
      if (this._target.getTrait("documentLoadingMarkers")) {
        await this.timelineFront.destroy();
        this.timelineFront = null;
      }

      resolve();
      this._connected = false;
    });
    return this._disconnection;
  },

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
      return new Promise((resolve) => {
        this._target.once("will-navigate", () => {
          this._target.once("navigate", () => {
            resolve();
          });
        });
      });
    };

    // Reconfigures the tab, optionally triggering a reload.
    let reconfigureTab = options => {
      return new Promise((resolve) => {
        this._target.activeTab.reconfigure(options, resolve);
      });
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
    return Promise.reject(new Error("Invalid activity type"));
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
    return new Promise((resolve) => {
      let request = null;
      let inspector = function () {
        request = getDisplayedRequestById(gStore.getState(), requestId);
        if (!request) {
          // Reset filters so that the request is visible.
          gStore.dispatch(Actions.toggleRequestFilterType("all"));
          request = getDisplayedRequestById(gStore.getState(), requestId);
        }

        // If the request was found, select it. Otherwise this function will be
        // called again once new requests arrive.
        if (request) {
          window.off(EVENTS.REQUEST_ADDED, inspector);
          gStore.dispatch(Actions.selectRequest(request.id));
          resolve();
        }
      };

      inspector();
      if (!request) {
        window.on(EVENTS.REQUEST_ADDED, inspector);
      }
    });
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
    return this.toolbox.viewSourceInDebugger(sourceURL, sourceLine);
  },

  /**
   * Start monitoring all incoming update events about network requests and wait until
   * a complete info about all requests is received. (We wait for the timings info
   * explicitly, because that's always the last piece of information that is received.)
   *
   * This method is designed to wait for network requests that are issued during a page
   * load, when retrieving page resources (scripts, styles, images). It has certain
   * assumptions that can make it unsuitable for other types of network communication:
   * - it waits for at least one network request to start and finish before returning
   * - it waits only for request that were issued after it was called. Requests that are
   *   already in mid-flight will be ignored.
   * - the request start and end times are overlapping. If a new request starts a moment
   *   after the previous one was finished, the wait will be ended in the "interim"
   *   period.
   * @returns a promise that resolves when the wait is done.
   * TODO: should be unified with whenDataAvailable in netmonitor-view.js
   */
  waitForAllRequestsFinished() {
    return new Promise(resolve => {
      // Key is the request id, value is a boolean - is request finished or not?
      let requests = new Map();

      function onRequest(_, id) {
        requests.set(id, false);
      }

      function onTimings(_, id) {
        requests.set(id, true);
        maybeResolve();
      }

      function maybeResolve() {
        // Have all the requests in the map finished yet?
        if (![...requests.values()].every(finished => finished)) {
          return;
        }

        // All requests are done - unsubscribe from events and resolve!
        window.off(EVENTS.NETWORK_EVENT, onRequest);
        window.off(EVENTS.RECEIVED_EVENT_TIMINGS, onTimings);
        resolve();
      }

      window.on(EVENTS.NETWORK_EVENT, onRequest);
      window.on(EVENTS.RECEIVED_EVENT_TIMINGS, onTimings);
    });
  },
};

/**
 * Functions handling target network events.
 */
function NetworkEventsHandler() {
  this.addRequest = this.addRequest.bind(this);
  this.updateRequest = this.updateRequest.bind(this);
  this.getString = this.getString.bind(this);
  this._onNetworkEvent = this._onNetworkEvent.bind(this);
  this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
  this._onDocLoadingMarker = this._onDocLoadingMarker.bind(this);
  this._onRequestHeaders = this._onRequestHeaders.bind(this);
  this._onRequestCookies = this._onRequestCookies.bind(this);
  this._onRequestPostData = this._onRequestPostData.bind(this);
  this._onResponseHeaders = this._onResponseHeaders.bind(this);
  this._onResponseCookies = this._onResponseCookies.bind(this);
  this._onResponseContent = this._onResponseContent.bind(this);
  this._onSecurityInfo = this._onSecurityInfo.bind(this);
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

  /**
   * Connect to the current target client.
   */
  connect: function () {
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
    gStore.dispatch(Actions.addTimingMarker(marker));
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

    this.addRequest(
      actor, {startedDateTime, method, url, isXHR, cause, fromCache, fromServiceWorker}
    );
    window.emit(EVENTS.NETWORK_EVENT, actor);
  },

  addRequest(id, data) {
    let { method, url, isXHR, cause, startedDateTime, fromCache,
          fromServiceWorker } = data;

    gStore.dispatch(Actions.addRequest(
      id,
      {
        // Convert the received date/time string to a unix timestamp.
        startedMillis: Date.parse(startedDateTime),
        method,
        url,
        isXHR,
        cause,
        fromCache,
        fromServiceWorker,
      },
      true
    ))
    .then(() => window.emit(EVENTS.REQUEST_ADDED, id));
  },

  async updateRequest(id, data) {
    const action = Actions.updateRequest(id, data, true);
    await gStore.dispatch(action);
    let {
      responseContent,
      responseCookies,
      responseHeaders,
      requestCookies,
      requestHeaders,
      requestPostData,
    } = action.data;
    let request = getRequestById(gStore.getState(), action.id);

    if (requestHeaders && requestHeaders.headers && requestHeaders.headers.length) {
      let headers = await fetchHeaders(requestHeaders, this.getString);
      if (headers) {
        await gStore.dispatch(Actions.updateRequest(
          action.id,
          { requestHeaders: headers },
          true,
        ));
      }
    }

    if (responseHeaders && responseHeaders.headers && responseHeaders.headers.length) {
      let headers = await fetchHeaders(responseHeaders, this.getString);
      if (headers) {
        await gStore.dispatch(Actions.updateRequest(
          action.id,
          { responseHeaders: headers },
          true,
        ));
      }
    }

    if (request && responseContent && responseContent.content) {
      let { mimeType } = request;
      let { text, encoding } = responseContent.content;
      let response = await this.getString(text);
      let payload = {};

      if (mimeType.includes("image/")) {
        payload.responseContentDataUri = formDataURI(mimeType, encoding, response);
      }

      responseContent.content.text = response;
      payload.responseContent = responseContent;

      await gStore.dispatch(Actions.updateRequest(action.id, payload, true));

      if (mimeType.includes("image/")) {
        window.emit(EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);
      }
    }

    // Search the POST data upload stream for request headers and add
    // them as a separate property, different from the classic headers.
    if (requestPostData && requestPostData.postData) {
      let { text } = requestPostData.postData;
      let postData = await this.getString(text);
      const headers = CurlUtils.getHeadersFromMultipartText(postData);
      const headersSize = headers.reduce((acc, { name, value }) => {
        return acc + name.length + value.length + 2;
      }, 0);
      let payload = {};
      requestPostData.postData.text = postData;
      payload.requestPostData = Object.assign({}, requestPostData);
      payload.requestHeadersFromUploadStream = { headers, headersSize };

      await gStore.dispatch(Actions.updateRequest(action.id, payload, true));
    }

    // Fetch request and response cookies long value.
    // Actor does not provide full sized cookie value when the value is too long
    // To display values correctly, we need fetch them in each request.
    if (requestCookies) {
      let reqCookies = [];
      // request store cookies in requestCookies or requestCookies.cookies
      let cookies = requestCookies.cookies ?
        requestCookies.cookies : requestCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (let cookie of cookies) {
          reqCookies.push(Object.assign({}, cookie, {
            value: await this.getString(cookie.value),
          }));
        }
        if (reqCookies.length) {
          await gStore.dispatch(Actions.updateRequest(
            action.id,
            { requestCookies: reqCookies },
            true));
        }
      }
    }

    if (responseCookies) {
      let resCookies = [];
      // response store cookies in responseCookies or responseCookies.cookies
      let cookies = responseCookies.cookies ?
        responseCookies.cookies : responseCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (let cookie of cookies) {
          resCookies.push(Object.assign({}, cookie, {
            value: await this.getString(cookie.value),
          }));
        }
        if (resCookies.length) {
          await gStore.dispatch(Actions.updateRequest(
            action.id,
            { responseCookies: resCookies },
            true));
        }
      }
    }
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
        this.updateRequest(actor, {
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
        this.updateRequest(actor, {
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
        this.updateRequest(actor, {
          contentSize: networkInfo.response.bodySize,
          transferredSize: networkInfo.response.transferredSize,
          mimeType: networkInfo.response.content.mimeType
        });
        this.webConsoleClient.getResponseContent(actor,
          this._onResponseContent);
        window.emit(EVENTS.UPDATING_RESPONSE_CONTENT, actor);
        break;
      case "eventTimings":
        this.updateRequest(actor, {
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
    this.updateRequest(response.from, {
      requestHeaders: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      requestCookies: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      requestPostData: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      securityInfo: response.securityInfo
    }).then(() => {
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
    this.updateRequest(response.from, {
      responseHeaders: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      responseCookies: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      responseContent: response
    }).then(() => {
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
    this.updateRequest(response.from, {
      eventTimings: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_EVENT_TIMINGS, response.from);
    });
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
    // FIXME: this.webConsoleClient will be undefined in mochitest,
    // so we return string instantly to skip undefined error
    if (typeof stringGrip === "string") {
      return Promise.resolve(stringGrip);
    }

    return this.webConsoleClient.getString(stringGrip);
  }
};

exports.NetMonitorController = NetMonitorController;
