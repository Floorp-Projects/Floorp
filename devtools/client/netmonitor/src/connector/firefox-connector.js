/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { CurlUtils } = require("devtools/client/shared/curl");
const { TimelineFront } = require("devtools/shared/fronts/timeline");
const { ACTIVITY_TYPE, EVENTS } = require("../constants");
const { getDisplayedRequestById } = require("../selectors/index");
const { fetchHeaders, formDataURI } = require("../utils/request-utils");

class FirefoxConnector {
  constructor() {
    this.connect = this.connect.bind(this);
    this.disconnect = this.disconnect.bind(this);
    this.willNavigate = this.willNavigate.bind(this);
    this.displayCachedEvents = this.displayCachedEvents.bind(this);
    this.onDocLoadingMarker = this.onDocLoadingMarker.bind(this);
    this.addRequest = this.addRequest.bind(this);
    this.updateRequest = this.updateRequest.bind(this);
    this.fetchImage = this.fetchImage.bind(this);
    this.fetchRequestHeaders = this.fetchRequestHeaders.bind(this);
    this.fetchResponseHeaders = this.fetchResponseHeaders.bind(this);
    this.fetchPostData = this.fetchPostData.bind(this);
    this.fetchResponseCookies = this.fetchResponseCookies.bind(this);
    this.fetchRequestCookies = this.fetchRequestCookies.bind(this);
    this.getPayloadFromQueue = this.getPayloadFromQueue.bind(this);
    this.isQueuePayloadReady = this.isQueuePayloadReady.bind(this);
    this.pushPayloadToQueue = this.pushPayloadToQueue.bind(this);
    this.sendHTTPRequest = this.sendHTTPRequest.bind(this);
    this.setPreferences = this.setPreferences.bind(this);
    this.triggerActivity = this.triggerActivity.bind(this);
    this.inspectRequest = this.inspectRequest.bind(this);
    this.getLongString = this.getLongString.bind(this);
    this.getNetworkRequest = this.getNetworkRequest.bind(this);
    this.getTabTarget = this.getTabTarget.bind(this);
    this.viewSourceInDebugger = this.viewSourceInDebugger.bind(this);

    // Event handlers
    this.onNetworkEvent = this.onNetworkEvent.bind(this);
    this.onNetworkEventUpdate = this.onNetworkEventUpdate.bind(this);
    this.onRequestHeaders = this.onRequestHeaders.bind(this);
    this.onRequestCookies = this.onRequestCookies.bind(this);
    this.onRequestPostData = this.onRequestPostData.bind(this);
    this.onSecurityInfo = this.onSecurityInfo.bind(this);
    this.onResponseHeaders = this.onResponseHeaders.bind(this);
    this.onResponseCookies = this.onResponseCookies.bind(this);
    this.onResponseContent = this.onResponseContent.bind(this);
    this.onEventTimings = this.onEventTimings.bind(this);
  }

  async connect(connection, actions, getState) {
    this.actions = actions;
    this.getState = getState;
    this.tabTarget = connection.tabConnection.tabTarget;
    this.tabClient = this.tabTarget.isTabActor ? this.tabTarget.activeTab : null;
    this.webConsoleClient = this.tabTarget.activeConsole;

    this.tabTarget.on("will-navigate", this.willNavigate);
    this.tabTarget.on("close", this.disconnect);
    this.webConsoleClient.on("networkEvent", this.onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this.onNetworkEventUpdate);

    // Don't start up waiting for timeline markers if the server isn't
    // recent enough to emit the markers we're interested in.
    if (this.tabTarget.getTrait("documentLoadingMarkers")) {
      this.timelineFront = new TimelineFront(this.tabTarget.client, this.tabTarget.form);
      this.timelineFront.on("doc-loading", this.onDocLoadingMarker);
      await this.timelineFront.start({ withDocLoadingEvents: true });
    }

    this.displayCachedEvents();
  }

  async disconnect() {
    // When debugging local or a remote instance, the connection is closed by
    // the RemoteTarget. The webconsole actor is stopped on disconnect.
    this.tabClient = null;
    this.webConsoleClient = null;

    // The timeline front wasn't initialized and started if the server wasn't
    // recent enough to emit the markers we were interested in.
    if (this.tabTarget.getTrait("documentLoadingMarkers") && this.timelineFront) {
      this.timelineFront.off("doc-loading", this.onDocLoadingMarker);
      await this.timelineFront.destroy();
      this.timelineFront = null;
    }
  }

  willNavigate() {
    if (!Services.prefs.getBoolPref("devtools.webconsole.persistlog")) {
      this.actions.batchReset();
      this.actions.clearRequests();
    } else {
      // If the log is persistent, just clear all accumulated timing markers.
      this.actions.clearTimingMarkers();
    }
  }

  /**
   * Display any network events already in the cache.
   */
  displayCachedEvents() {
    for (let networkInfo of this.webConsoleClient.getNetworkEvents()) {
      // First add the request to the timeline.
      this.onNetworkEvent("networkEvent", networkInfo);
      // Then replay any updates already received.
      for (let updateType of networkInfo.updates) {
        this.onNetworkEventUpdate("networkEventUpdate", {
          packet: { updateType },
          networkInfo,
        });
      }
    }
  }

  /**
   * The "DOMContentLoaded" and "Load" events sent by the timeline actor.
   *
   * @param {object} marker
   */
  onDocLoadingMarker(marker) {
    window.emit(EVENTS.TIMELINE_EVENT, marker);
    this.actions.addTimingMarker(marker);
  }

  /**
   * Add a new network request to application state.
   *
   * @param {string} id request id
   * @param {object} data data payload will be added to application state
   */
  addRequest(id, data) {
    let {
      method,
      url,
      isXHR,
      cause,
      startedDateTime,
      fromCache,
      fromServiceWorker,
    } = data;

    this.actions.addRequest(
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
      true,
    )
    .then(() => window.emit(EVENTS.REQUEST_ADDED, id));
  }

  /**
   * Update a network request if it already exists in application state.
   *
   * @param {string} id request id
   * @param {object} data data payload will be updated to application state
   */
  async updateRequest(id, data) {
    let {
      mimeType,
      responseContent,
      responseCookies,
      responseHeaders,
      requestCookies,
      requestHeaders,
      requestPostData,
    } = data;

    // fetch request detail contents in parallel
    let [
      imageObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj,
    ] = await Promise.all([
      this.fetchImage(mimeType, responseContent),
      this.fetchRequestHeaders(requestHeaders),
      this.fetchResponseHeaders(responseHeaders),
      this.fetchPostData(requestPostData),
      this.fetchRequestCookies(requestCookies),
      this.fetchResponseCookies(responseCookies),
    ]);

    let payload = Object.assign({}, data,
                                    imageObj, requestHeadersObj, responseHeadersObj,
                                    postDataObj, requestCookiesObj, responseCookiesObj);
    await this.actions.updateRequest(id, payload, true);
  }

  async fetchImage(mimeType, responseContent) {
    let payload = {};
    if (mimeType && responseContent && responseContent.content) {
      let { encoding, text } = responseContent.content;
      let response = await this.getLongString(text);

      if (mimeType.includes("image/")) {
        payload.responseContentDataUri = formDataURI(mimeType, encoding, response);
      }

      responseContent.content.text = response;
      payload.responseContent = responseContent;
    }
    return payload;
  }

  async fetchRequestHeaders(requestHeaders) {
    let payload = {};
    if (requestHeaders && requestHeaders.headers && requestHeaders.headers.length) {
      let headers = await fetchHeaders(requestHeaders, this.getLongString);
      if (headers) {
        payload.requestHeaders = headers;
      }
    }
    return payload;
  }

  async fetchResponseHeaders(responseHeaders) {
    let payload = {};
    if (responseHeaders && responseHeaders.headers && responseHeaders.headers.length) {
      let headers = await fetchHeaders(responseHeaders, this.getLongString);
      if (headers) {
        payload.responseHeaders = headers;
      }
    }
    return payload;
  }

  async fetchPostData(requestPostData) {
    let payload = {};
    if (requestPostData && requestPostData.postData) {
      let { text } = requestPostData.postData;
      let postData = await this.getLongString(text);
      const headers = CurlUtils.getHeadersFromMultipartText(postData);
      const headersSize = headers.reduce((acc, { name, value }) => {
        return acc + name.length + value.length + 2;
      }, 0);
      requestPostData.postData.text = postData;
      payload.requestPostData = Object.assign({}, requestPostData);
      payload.requestHeadersFromUploadStream = { headers, headersSize };
    }
    return payload;
  }

  async fetchResponseCookies(responseCookies) {
    let payload = {};
    if (responseCookies) {
      let resCookies = [];
      // response store cookies in responseCookies or responseCookies.cookies
      let cookies = responseCookies.cookies ?
        responseCookies.cookies : responseCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (let cookie of cookies) {
          resCookies.push(Object.assign({}, cookie, {
            value: await this.getLongString(cookie.value),
          }));
        }
        if (resCookies.length) {
          payload.responseCookies = resCookies;
        }
      }
    }
    return payload;
  }

  async fetchRequestCookies(requestCookies) {
    let payload = {};
    if (requestCookies) {
      let reqCookies = [];
      // request store cookies in requestCookies or requestCookies.cookies
      let cookies = requestCookies.cookies ?
        requestCookies.cookies : requestCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (let cookie of cookies) {
          reqCookies.push(Object.assign({}, cookie, {
            value: await this.getLongString(cookie.value),
          }));
        }
        if (reqCookies.length) {
          payload.requestCookies = reqCookies;
        }
      }
    }
    return payload;
  }

  /**
   * Access a payload item from payload queue.
   *
   * @param {string} id request id
   * @return {boolean} return a queued payload item from queue.
   */
  getPayloadFromQueue(id) {
    return this.payloadQueue.find((item) => item.id === id);
  }

  /**
   * Packet order of "networkUpdateEvent" is predictable, as a result we can wait for
   * the last one "eventTimings" packet arrives to check payload is ready.
   *
   * @param {string} id request id
   * @return {boolean} return whether a specific networkEvent has been updated completely.
   */
  isQueuePayloadReady(id) {
    let queuedPayload = this.getPayloadFromQueue(id);
    return queuedPayload && queuedPayload.payload.eventTimings;
  }

  /**
   * Push a request payload into a queue if request doesn't exist. Otherwise update the
   * request itself.
   *
   * @param {string} id request id
   * @param {object} payload request data payload
   */
  pushPayloadToQueue(id, payload) {
    let queuedPayload = this.getPayloadFromQueue(id);
    if (!queuedPayload) {
      this.payloadQueue.push({ id, payload });
    } else {
      // Merge upcoming networkEventUpdate payload into existing one
      queuedPayload.payload = Object.assign({}, queuedPayload.payload, payload);
    }
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
   * Selects the specified request in the waterfall and opens the details view.
   *
   * @param {string} requestId The actor ID of the request to inspect.
   * @return {object} A promise resolved once the task finishes.
   */
  inspectRequest(requestId) {
    // Look for the request in the existing ones or wait for it to appear, if
    // the network monitor is still loading.
    return new Promise((resolve) => {
      let request = null;
      let inspector = () => {
        request = getDisplayedRequestById(this.getState(), requestId);
        if (!request) {
          // Reset filters so that the request is visible.
          this.actions.toggleRequestFilterType("all");
          request = getDisplayedRequestById(this.getState(), requestId);
        }

        // If the request was found, select it. Otherwise this function will be
        // called again once new requests arrive.
        if (request) {
          window.off(EVENTS.REQUEST_ADDED, inspector);
          this.actions.selectRequest(request.id);
          resolve();
        }
      };

      inspector();

      if (!request) {
        window.on(EVENTS.REQUEST_ADDED, inspector);
      }
    });
  }

  /**
   * Fetches the network information packet from actor server
   *
   * @param {string} id request id
   * @return {object} networkInfo data packet
   */
  getNetworkRequest(id) {
    return this.webConsoleClient.getNetworkRequest(id);
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
    return this.webConsoleClient.getString(stringGrip);
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
   * The "networkEvent" message type handler.
   *
   * @param {string} type message type
   * @param {object} networkInfo network request information
   */
  onNetworkEvent(type, networkInfo) {
    let {
      actor,
      cause,
      fromCache,
      fromServiceWorker,
      isXHR,
      request: {
        method,
        url,
      },
      startedDateTime,
    } = networkInfo;

    this.addRequest(actor, {
      cause,
      fromCache,
      fromServiceWorker,
      isXHR,
      method,
      startedDateTime,
      url,
    });

    window.emit(EVENTS.NETWORK_EVENT, actor);
  }

  /**
   * The "networkEventUpdate" message type handler.
   *
   * @param {string} type message type
   * @param {object} packet the message received from the server.
   * @param {object} networkInfo the network request information.
   */
  onNetworkEventUpdate(type, { packet, networkInfo }) {
    let { actor } = networkInfo;

    switch (packet.updateType) {
      case "requestHeaders":
        this.webConsoleClient.getRequestHeaders(actor, this.onRequestHeaders);
        window.emit(EVENTS.UPDATING_REQUEST_HEADERS, actor);
        break;
      case "requestCookies":
        this.webConsoleClient.getRequestCookies(actor, this.onRequestCookies);
        window.emit(EVENTS.UPDATING_REQUEST_COOKIES, actor);
        break;
      case "requestPostData":
        this.webConsoleClient.getRequestPostData(actor, this.onRequestPostData);
        window.emit(EVENTS.UPDATING_REQUEST_POST_DATA, actor);
        break;
      case "securityInfo":
        this.updateRequest(actor, {
          securityState: networkInfo.securityInfo,
        }).then(() => {
          this.webConsoleClient.getSecurityInfo(actor, this.onSecurityInfo);
          window.emit(EVENTS.UPDATING_SECURITY_INFO, actor);
        });
        break;
      case "responseHeaders":
        this.webConsoleClient.getResponseHeaders(actor, this.onResponseHeaders);
        window.emit(EVENTS.UPDATING_RESPONSE_HEADERS, actor);
        break;
      case "responseCookies":
        this.webConsoleClient.getResponseCookies(actor, this.onResponseCookies);
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
        }).then(() => {
          window.emit(EVENTS.STARTED_RECEIVING_RESPONSE, actor);
        });
        break;
      case "responseContent":
        this.webConsoleClient.getResponseContent(actor,
          this.onResponseContent.bind(this, {
            contentSize: networkInfo.response.bodySize,
            transferredSize: networkInfo.response.transferredSize,
            mimeType: networkInfo.response.content.mimeType
          }));
        window.emit(EVENTS.UPDATING_RESPONSE_CONTENT, actor);
        break;
      case "eventTimings":
        this.updateRequest(actor, { totalTime: networkInfo.totalTime })
          .then(() => {
            this.webConsoleClient.getEventTimings(actor, this.onEventTimings);
            window.emit(EVENTS.UPDATING_EVENT_TIMINGS, actor);
          });
        break;
    }
  }

  /**
   * Handles additional information received for a "requestHeaders" packet.
   *
   * @param {object} response the message received from the server.
   */
  onRequestHeaders(response) {
    this.updateRequest(response.from, {
      requestHeaders: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_REQUEST_HEADERS, response.from);
    });
  }

  /**
   * Handles additional information received for a "requestCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  onRequestCookies(response) {
    this.updateRequest(response.from, {
      requestCookies: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_REQUEST_COOKIES, response.from);
    });
  }

  /**
   * Handles additional information received for a "requestPostData" packet.
   *
   * @param {object} response the message received from the server.
   */
  onRequestPostData(response) {
    this.updateRequest(response.from, {
      requestPostData: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_REQUEST_POST_DATA, response.from);
    });
  }

  /**
   * Handles additional information received for a "securityInfo" packet.
   *
   * @param {object} response the message received from the server.
   */
  onSecurityInfo(response) {
    this.updateRequest(response.from, {
      securityInfo: response.securityInfo
    }).then(() => {
      window.emit(EVENTS.RECEIVED_SECURITY_INFO, response.from);
    });
  }

  /**
   * Handles additional information received for a "responseHeaders" packet.
   *
   * @param {object} response the message received from the server.
   */
  onResponseHeaders(response) {
    this.updateRequest(response.from, {
      responseHeaders: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_RESPONSE_HEADERS, response.from);
    });
  }

  /**
   * Handles additional information received for a "responseCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  onResponseCookies(response) {
    this.updateRequest(response.from, {
      responseCookies: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_RESPONSE_COOKIES, response.from);
    });
  }

  /**
   * Handles additional information received for a "responseContent" packet.
   *
   * @param {object} data the message received from the server event.
   * @param {object} response the message received from the server.
   */
  onResponseContent(data, response) {
    let payload = Object.assign({ responseContent: response }, data);
    this.updateRequest(response.from, payload).then(() => {
      window.emit(EVENTS.RECEIVED_RESPONSE_CONTENT, response.from);
    });
  }

  /**
   * Handles additional information received for a "eventTimings" packet.
   *
   * @param {object} response the message received from the server.
   */
  onEventTimings(response) {
    this.updateRequest(response.from, {
      eventTimings: response
    }).then(() => {
      window.emit(EVENTS.RECEIVED_EVENT_TIMINGS, response.from);
    });
  }
}

module.exports = new FirefoxConnector();
