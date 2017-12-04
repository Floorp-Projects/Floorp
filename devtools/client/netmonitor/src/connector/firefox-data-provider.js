/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable block-scoped-var */

"use strict";

const { EVENTS } = require("../constants");
const { CurlUtils } = require("devtools/client/shared/curl");
const { fetchHeaders } = require("../utils/request-utils");

/**
 * This object is responsible for fetching additional HTTP
 * data from the backend over RDP protocol.
 *
 * The object also keeps track of RDP requests in-progress,
 * so it's possible to determine whether all has been fetched
 * or not.
 */
class FirefoxDataProvider {
  constructor({webConsoleClient, actions}) {
    // Options
    this.webConsoleClient = webConsoleClient;
    this.actions = actions;

    // Internal properties
    this.payloadQueue = [];
    this.rdpRequestMap = new Map();

    // Map[key string => Promise] used by `requestData` to prevent requesting the same
    // request data twice.
    this.lazyRequestData = new Map();

    // Fetching data from the backend
    this.getLongString = this.getLongString.bind(this);
    this.getRequestFromQueue = this.getRequestFromQueue.bind(this);

    // Event handlers
    this.onNetworkEvent = this.onNetworkEvent.bind(this);
    this.onNetworkEventUpdate = this.onNetworkEventUpdate.bind(this);
  }

  /**
   * Add a new network request to application state.
   *
   * @param {string} id request id
   * @param {object} data data payload will be added to application state
   */
  async addRequest(id, data) {
    let {
      method,
      url,
      isXHR,
      cause,
      startedDateTime,
      fromCache,
      fromServiceWorker,
    } = data;

    if (this.actions.addRequest) {
      await this.actions.addRequest(id, {
        // Convert the received date/time string to a unix timestamp.
        startedMillis: Date.parse(startedDateTime),
        method,
        url,
        isXHR,
        cause,

        // Compatibility code to support Firefox 58 and earlier that always
        // send stack-trace immediately on networkEvent message.
        // FF59+ supports fetching the traces lazily via requestData.
        stacktrace: cause.stacktrace,

        fromCache,
        fromServiceWorker},
        true,
      );
    }

    emit(EVENTS.REQUEST_ADDED, id);
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
      responseContentObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj,
    ] = await Promise.all([
      this.fetchResponseContent(mimeType, responseContent),
      this.fetchRequestHeaders(requestHeaders),
      this.fetchResponseHeaders(responseHeaders),
      this.fetchPostData(requestPostData),
      this.fetchRequestCookies(requestCookies),
      this.fetchResponseCookies(responseCookies),
    ]);

    let payload = Object.assign({},
      data,
      responseContentObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj
    );

    this.pushRequestToQueue(id, payload);

    return payload;
  }

  async fetchResponseContent(mimeType, responseContent) {
    let payload = {};
    if (mimeType && responseContent && responseContent.content) {
      let { text } = responseContent.content;
      let response = await this.getLongString(text);
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

      // Calculate total header size and don't forget to include
      // two new-line characters at the end.
      const headersSize = headers.reduce((acc, { name, value }) => {
        return acc + name.length + value.length + 2;
      }, 0);

      requestPostData.postData.text = postData;
      payload.requestPostData = Object.assign({}, requestPostData);
      payload.requestHeadersFromUploadStream = { headers, headersSize };

      // Lock down requestPostDataAvailable once we fetch data from back-end.
      // Using this as flag to prevent fetching arrived data again.
      payload.requestPostDataAvailable = false;
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

      // Lock down responseCookiesAvailable once we fetch data from back-end.
      // Using this as flag to prevent fetching arrived data again.
      payload.responseCookiesAvailable = false;
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

      // Lock down requestCookiesAvailable once we fetch data from back-end.
      // Using this as flag to prevent fetching arrived data again.
      payload.requestCookiesAvailable = false;
    }
    return payload;
  }

  /**
   * Access a payload item from payload queue.
   *
   * @param {string} id request id
   * @return {boolean} return a queued payload item from queue.
   */
  getRequestFromQueue(id) {
    return this.payloadQueue.find((item) => item.id === id);
  }

  /**
   * Public API used by the Toolbox: Tells if there is still any pending request.
   *
   * @return {boolean} returns true if the payload queue is empty
   */
  isPayloadQueueEmpty() {
    return this.payloadQueue.length === 0;
  }

  /**
   * Return true if payload is ready (all data fetched from the backend)
   *
   * @param {string} id request id
   * @return {boolean} return whether a specific networkEvent has been updated completely.
   */
  isRequestPayloadReady(id) {
    let record = this.rdpRequestMap.get(id);
    if (!record) {
      return false;
    }

    let { payload } = this.getRequestFromQueue(id);

    // The payload is ready when all values in the record are true.
    // Note that we never fetch response header/cookies for request with security issues.
    // Bug 1404917 should simplify this heuristic by making all these field be lazily
    // fetched, only on-demand.
    return record.requestHeaders && record.eventTimings &&
      (record.responseHeaders || payload.securityState === "broken" ||
        (!payload.status && payload.responseContentAvailable));
  }

  /**
   * Merge upcoming networkEventUpdate payload into existing one.
   *
   * @param {string} id request id
   * @param {object} payload request data payload
   */
  pushRequestToQueue(id, payload) {
    let request = this.getRequestFromQueue(id);
    if (!request) {
      this.payloadQueue.push({ id, payload });
    } else {
      // Merge upcoming networkEventUpdate payload into existing one
      request.payload = Object.assign({}, request.payload, payload);
    }
  }

  cleanUpQueue(id) {
    this.payloadQueue = this.payloadQueue.filter(
      request => request.id != id);
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

    // Create tracking record for this request.
    this.rdpRequestMap.set(actor, {
      requestHeaders: false,
      responseHeaders: false,
      eventTimings: false,
    });

    this.addRequest(actor, {
      cause,
      fromCache,
      fromServiceWorker,
      isXHR,
      method,
      startedDateTime,
      url,
    });

    emit(EVENTS.NETWORK_EVENT, actor);
  }

  /**
   * The "networkEventUpdate" message type handler.
   *
   * @param {string} type message type
   * @param {object} packet the message received from the server.
   * @param {object} networkInfo the network request information.
   */
  onNetworkEventUpdate(type, data) {
    let { packet, networkInfo } = data;
    let { actor } = networkInfo;
    let { updateType } = packet;

    // When we pause and resume, we may receive `networkEventUpdate` for a request
    // that started during the pause and we missed its `networkEvent`.
    if (!this.rdpRequestMap.has(actor)) {
      return;
    }

    switch (updateType) {
      case "requestHeaders":
      case "responseHeaders":
        this.requestPayloadData(actor, updateType);
        break;
      case "requestCookies":
      case "responseCookies":
      case "requestPostData":
        // This field helps knowing when/if updateType property is available
        // and can be requested via `requestData`
        this.updateRequest(actor, { [`${updateType}Available`]: true });
        break;
      case "securityInfo":
        this.updateRequest(actor, { securityState: networkInfo.securityInfo });
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
          emit(EVENTS.STARTED_RECEIVING_RESPONSE, actor);
        });
        break;
      case "responseContent":
        this.updateRequest(actor, {
          contentSize: networkInfo.response.bodySize,
          transferredSize: networkInfo.response.transferredSize,
          mimeType: networkInfo.response.content.mimeType,
          // This field helps knowing when/if responseContent property is available
          // and can be requested via `requestData`
          responseContentAvailable: true,
        });
        break;
      case "eventTimings":
        this.pushRequestToQueue(actor, { totalTime: networkInfo.totalTime });
        this.requestPayloadData(actor, updateType);
        break;
    }

    emit(EVENTS.NETWORK_EVENT_UPDATED, actor);
  }

  /**
   * Wrapper method for requesting HTTP details data for the payload.
   *
   * It is specific to all requests done from `onNetworkEventUpdate`, for data that are
   * immediately fetched whenever the data is available.
   *
   * All these requests are cached into `rdpRequestMap`. All requests related to a given
   * actor will be collected in the same record.
   *
   * Once bug 1404917 is completed, we should no longer use this method.
   * All request fields should be loaded only on-demand, via `requestData` method.
   *
   * @param {string} actor actor id (used as request id)
   * @param {string} method identifier of the data we want to fetch
   */
  requestPayloadData(actor, method) {
    let record = this.rdpRequestMap.get(actor);

    // If data has been already requested, do nothing.
    if (record[method]) {
      return;
    }

    let promise = this._requestData(actor, method);
    promise.then(() => {
      // Once we got the data toggle the Map item to `true` in order to
      // make isRequestPayloadReady return `true` once all the data is fetched.
      record[method] = true;
      this.onPayloadDataReceived(actor, method, !record);
    });
  }

  /**
   * Executed when new data are received from the backend.
   */
  async onPayloadDataReceived(actor, type) {
    // Notify actions when all the sync request from onNetworkEventUpdate are done,
    // or, everytime requestData is called for fetching data lazily.
    if (this.isRequestPayloadReady(actor)) {
      let payloadFromQueue = this.getRequestFromQueue(actor).payload;

      // Clean up
      this.cleanUpQueue(actor);
      this.rdpRequestMap.delete(actor);

      if (this.actions.updateRequest) {
        await this.actions.updateRequest(actor, payloadFromQueue, true);
      }

      // This event is fired only once per request, once all the properties are fetched
      // from `onNetworkEventUpdate`. There should be no more RDP requests after this.
      emit(EVENTS.PAYLOAD_READY, actor);
    }
  }

  /**
   * Public connector API to lazily request HTTP details from the backend.
   *
   * The method focus on:
   * - calling the right actor method,
   * - emitting an event to tell we start fetching some request data,
   * - call data processing method.
   *
   * @param {string} actor actor id (used as request id)
   * @param {string} method identifier of the data we want to fetch
   *
   * @return {Promise} return a promise resolved when data is received.
   */
  requestData(actor, method) {
    // Key string used in `lazyRequestData`. We use this Map to prevent requesting
    // the same data twice at the same time.
    let key = `${actor}-${method}`;
    let promise = this.lazyRequestData.get(key);
    // If a request is pending, reuse it.
    if (promise) {
      return promise;
    }
    // Fetch the data
    promise = this._requestData(actor, method);
    this.lazyRequestData.set(key, promise);
    promise.then(async () => {
      // Remove the request from the cache, any new call to requestData will fetch the
      // data again.
      this.lazyRequestData.delete(key, promise);

      if (this.actions.updateRequest) {
        await this.actions.updateRequest(
          actor,
          this.getRequestFromQueue(actor).payload,
          true,
        );
      }
    });
    return promise;
  }

  /**
   * Internal helper used to request HTTP details from the backend.
   *
   * This is internal method that focus on:
   * - calling the right actor method,
   * - emitting an event to tell we start fetching some request data,
   * - call data processing method.
   *
   * @param {string} actor actor id (used as request id)
   * @param {string} method identifier of the data we want to fetch
   *
   * @return {Promise} return a promise resolved when data is received.
   */
  async _requestData(actor, method) {
    // Calculate real name of the client getter.
    let clientMethodName = `get${method.charAt(0).toUpperCase()}${method.slice(1)}`;
    // The name of the callback that processes request response
    let callbackMethodName = `on${method.charAt(0).toUpperCase()}${method.slice(1)}`;
    // And the event to fire before updating this data
    let updatingEventName = `UPDATING_${method.replace(/([A-Z])/g, "_$1").toUpperCase()}`;

    // Emit event that tell we just start fetching some data
    emit(EVENTS[updatingEventName], actor);

    let response = await new Promise((resolve, reject) => {
      // Do a RDP request to fetch data from the actor.
      if (typeof this.webConsoleClient[clientMethodName] === "function") {
        // Make sure we fetch the real actor data instead of cloned actor
        // e.g. CustomRequestPanel will clone a request with additional '-clone' actor id
        this.webConsoleClient[clientMethodName](actor.replace("-clone", ""), (res) => {
          if (res.error) {
            console.error(res.message);
          }
          resolve(res);
        });
      } else {
        reject(new Error(`Error: No such client method '${clientMethodName}'!`));
      }
    });

    // Restore clone actor id
    if (actor.includes("-clone")) {
      // Because response's properties are read-only, we create a new response
      response = { ...response, from: `${response.from}-clone` };
    }

    // Call data processing method.
    return this[callbackMethodName](response);
  }

  /**
   * Handles additional information received for a "requestHeaders" packet.
   *
   * @param {object} response the message received from the server.
   */
  onRequestHeaders(response) {
    return this.updateRequest(response.from, {
      requestHeaders: response
    }).then(() => {
      emit(EVENTS.RECEIVED_REQUEST_HEADERS, response.from);
    });
  }

  /**
   * Handles additional information received for a "requestCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onRequestCookies(response) {
    let payload = await this.updateRequest(response.from, {
      requestCookies: response
    });
    emit(EVENTS.RECEIVED_REQUEST_COOKIES, response.from);
    return payload.requestCookies;
  }

  /**
   * Handles additional information received for a "requestPostData" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onRequestPostData(response) {
    let payload = await this.updateRequest(response.from, {
      requestPostData: response
    });
    emit(EVENTS.RECEIVED_REQUEST_POST_DATA, response.from);
    return payload;
  }

  /**
   * Handles additional information received for a "securityInfo" packet.
   *
   * @param {object} response the message received from the server.
   */
  onSecurityInfo(response) {
    return this.updateRequest(response.from, {
      securityInfo: response.securityInfo
    }).then(() => {
      emit(EVENTS.RECEIVED_SECURITY_INFO, response.from);
    });
  }

  /**
   * Handles additional information received for a "responseHeaders" packet.
   *
   * @param {object} response the message received from the server.
   */
  onResponseHeaders(response) {
    return this.updateRequest(response.from, {
      responseHeaders: response
    }).then(() => {
      emit(EVENTS.RECEIVED_RESPONSE_HEADERS, response.from);
    });
  }

  /**
   * Handles additional information received for a "responseCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onResponseCookies(response) {
    let payload = await this.updateRequest(response.from, {
      responseCookies: response
    });
    emit(EVENTS.RECEIVED_RESPONSE_COOKIES, response.from);
    return payload.responseCookies;
  }

  /**
   * Handles additional information received via "getResponseContent" request.
   *
   * @param {object} response the message received from the server.
   */
  async onResponseContent(response) {
    let payload = await this.updateRequest(response.from, {
      // We have to ensure passing mimeType as fetchResponseContent needs it from
      // updateRequest. It will convert the LongString in `response.content.text` to a
      // string.
      mimeType: response.content.mimeType,
      responseContent: response,
    });
    emit(EVENTS.RECEIVED_RESPONSE_CONTENT, response.from);
    return payload.responseContent;
  }

  /**
   * Handles additional information received for a "eventTimings" packet.
   *
   * @param {object} response the message received from the server.
   */
  onEventTimings(response) {
    return this.updateRequest(response.from, {
      eventTimings: response
    }).then(() => {
      emit(EVENTS.RECEIVED_EVENT_TIMINGS, response.from);
    });
  }

  /**
   * Handles information received for a "stackTrace" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onStackTrace(response) {
    let payload = await this.updateRequest(response.from, {
      stacktrace: response.stacktrace
    });
    emit(EVENTS.RECEIVED_EVENT_STACKTRACE, response.from);
    return payload.stacktrace;
  }
}

/**
 * Guard 'emit' to avoid exception in non-window environment.
 */
function emit(type, data) {
  if (typeof window != "undefined") {
    window.emit(type, data);
  }
}

module.exports = FirefoxDataProvider;
