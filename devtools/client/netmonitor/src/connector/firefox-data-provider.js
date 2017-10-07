/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable block-scoped-var */

"use strict";

const { EVENTS } = require("../constants");
const { CurlUtils } = require("devtools/client/shared/curl");
const {
  fetchHeaders,
  formDataURI,
} = require("../utils/request-utils");

/**
 * This object is responsible for fetching additional HTTP
 * data from the backend.
 */
class FirefoxDataProvider {
  constructor({webConsoleClient, actions}) {
    // Options
    this.webConsoleClient = webConsoleClient;
    this.actions = actions;

    // Internal properties
    this.payloadQueue = [];

    // Public methods
    this.addRequest = this.addRequest.bind(this);
    this.updateRequest = this.updateRequest.bind(this);

    // Internals
    this.fetchResponseBody = this.fetchResponseBody.bind(this);
    this.fetchRequestHeaders = this.fetchRequestHeaders.bind(this);
    this.fetchResponseHeaders = this.fetchResponseHeaders.bind(this);
    this.fetchPostData = this.fetchPostData.bind(this);
    this.fetchResponseCookies = this.fetchResponseCookies.bind(this);
    this.fetchRequestCookies = this.fetchRequestCookies.bind(this);
    this.getPayloadFromQueue = this.getPayloadFromQueue.bind(this);
    this.isQueuePayloadReady = this.isQueuePayloadReady.bind(this);
    this.pushPayloadToQueue = this.pushPayloadToQueue.bind(this);
    this.getLongString = this.getLongString.bind(this);
    this.getNetworkRequest = this.getNetworkRequest.bind(this);

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
      imageObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj,
    ] = await Promise.all([
      this.fetchResponseBody(mimeType, responseContent),
      this.fetchRequestHeaders(requestHeaders),
      this.fetchResponseHeaders(responseHeaders),
      this.fetchPostData(requestPostData),
      this.fetchRequestCookies(requestCookies),
      this.fetchResponseCookies(responseCookies),
    ]);

    let payload = Object.assign({},
      data,
      imageObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj
    );

    this.pushPayloadToQueue(id, payload);

    if (this.actions.updateRequest && this.isQueuePayloadReady(id)) {
      await this.actions.updateRequest(id, this.getPayloadFromQueue(id).payload, true);
    }
  }

  async fetchResponseBody(mimeType, responseContent) {
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

      // Calculate total header size and don't forget to include
      // two new-line characters at the end.
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
   * Return true if payload is ready (all data fetched from the backend)
   *
   * @param {string} id request id
   * @return {boolean} return whether a specific networkEvent has been updated completely.
   */
  isQueuePayloadReady(id) {
    let queuedPayload = this.getPayloadFromQueue(id);

    // TODO we should find a better solution since it might happen
    // that eventTimings is not the last update.
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
  onNetworkEventUpdate(type, { packet, networkInfo }) {
    let { actor } = networkInfo;

    switch (packet.updateType) {
      case "requestHeaders":
        this.webConsoleClient.getRequestHeaders(actor, this.onRequestHeaders);
        emit(EVENTS.UPDATING_REQUEST_HEADERS, actor);
        break;
      case "requestCookies":
        this.webConsoleClient.getRequestCookies(actor, this.onRequestCookies);
        emit(EVENTS.UPDATING_REQUEST_COOKIES, actor);
        break;
      case "requestPostData":
        this.webConsoleClient.getRequestPostData(actor, this.onRequestPostData);
        emit(EVENTS.UPDATING_REQUEST_POST_DATA, actor);
        break;
      case "securityInfo":
        this.updateRequest(actor, {
          securityState: networkInfo.securityInfo,
        }).then(() => {
          this.webConsoleClient.getSecurityInfo(actor, this.onSecurityInfo);
          emit(EVENTS.UPDATING_SECURITY_INFO, actor);
        });
        break;
      case "responseHeaders":
        this.webConsoleClient.getResponseHeaders(actor, this.onResponseHeaders);
        emit(EVENTS.UPDATING_RESPONSE_HEADERS, actor);
        break;
      case "responseCookies":
        this.webConsoleClient.getResponseCookies(actor, this.onResponseCookies);
        emit(EVENTS.UPDATING_RESPONSE_COOKIES, actor);
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
        this.webConsoleClient.getResponseContent(actor,
          this.onResponseContent.bind(this, {
            contentSize: networkInfo.response.bodySize,
            transferredSize: networkInfo.response.transferredSize,
            mimeType: networkInfo.response.content.mimeType
          }));
        emit(EVENTS.UPDATING_RESPONSE_CONTENT, actor);
        break;
      case "eventTimings":
        this.updateRequest(actor, { totalTime: networkInfo.totalTime })
          .then(() => {
            this.webConsoleClient.getEventTimings(actor, this.onEventTimings);
            emit(EVENTS.UPDATING_EVENT_TIMINGS, actor);
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
      emit(EVENTS.RECEIVED_REQUEST_HEADERS, response.from);
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
      emit(EVENTS.RECEIVED_REQUEST_COOKIES, response.from);
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
      emit(EVENTS.RECEIVED_REQUEST_POST_DATA, response.from);
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
      emit(EVENTS.RECEIVED_SECURITY_INFO, response.from);
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
      emit(EVENTS.RECEIVED_RESPONSE_HEADERS, response.from);
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
      emit(EVENTS.RECEIVED_RESPONSE_COOKIES, response.from);
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
      emit(EVENTS.RECEIVED_RESPONSE_CONTENT, response.from);
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
      emit(EVENTS.RECEIVED_EVENT_TIMINGS, response.from);
    });
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
