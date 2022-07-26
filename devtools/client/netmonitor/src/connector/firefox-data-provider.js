/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable block-scoped-var */

"use strict";

const {
  EVENTS,
  TEST_EVENTS,
} = require("devtools/client/netmonitor/src/constants");
const { CurlUtils } = require("devtools/client/shared/curl");
const {
  fetchHeaders,
} = require("devtools/client/netmonitor/src/utils/request-utils");

const {
  getLongStringFullText,
} = require("devtools/client/shared/string-utils");

/**
 * This object is responsible for fetching additional HTTP
 * data from the backend over RDP protocol.
 *
 * The object also keeps track of RDP requests in-progress,
 * so it's possible to determine whether all has been fetched
 * or not.
 */
class FirefoxDataProvider {
  /**
   * Constructor for data provider
   *
   * @param {Object} commands Object defined from devtools/shared/commands to interact with the devtools backend
   * @param {Object} actions set of actions fired during data fetching process.
   * @param {Object} owner all events are fired on this object.
   */
  constructor({ commands, actions, owner }) {
    // Options
    this.commands = commands;
    this.actions = actions || {};
    this.actionsEnabled = true;

    // Allow requesting of on-demand network data, this would be `false` when requests
    // are cleared (as we clear also on the backend), and will be flipped back
    // to true on the next `onNetworkResourceAvailable` call.
    this._requestDataEnabled = true;

    // `_requestDataEnabled` can only be used to prevent new calls to
    // requestData. For pending/already started calls, we need to check if
    // clear() was called during the call, which is the purpose of this counter.
    this._lastRequestDataClearId = 0;

    this.owner = owner;

    // This holds stacktraces infomation temporarily. Stacktrace resources
    // can come before or after (out of order) their related network events.
    // This will hold stacktrace related info from the NETWORK_EVENT_STACKTRACE resource
    // for the NETWORK_EVENT resource and vice versa.
    this.stackTraces = new Map();
    // Map of the stacktrace information keyed by the actor id's
    this.stackTraceRequestInfoByActorID = new Map();

    // For tracking unfinished requests
    this.pendingRequests = new Set();

    // Map[key string => Promise] used by `requestData` to prevent requesting the same
    // request data twice.
    this.lazyRequestData = new Map();

    // Fetching data from the backend
    this.getLongString = this.getLongString.bind(this);

    // Event handlers
    this.onNetworkResourceAvailable = this.onNetworkResourceAvailable.bind(
      this
    );
    this.onNetworkResourceUpdated = this.onNetworkResourceUpdated.bind(this);

    this.onWebSocketOpened = this.onWebSocketOpened.bind(this);
    this.onWebSocketClosed = this.onWebSocketClosed.bind(this);
    this.onFrameSent = this.onFrameSent.bind(this);
    this.onFrameReceived = this.onFrameReceived.bind(this);

    this.onEventSourceConnectionClosed = this.onEventSourceConnectionClosed.bind(
      this
    );
    this.onEventReceived = this.onEventReceived.bind(this);
    this.setEventStreamFlag = this.setEventStreamFlag.bind(this);
  }

  /*
   * Cleans up all the internal states, this usually done before navigation
   * (without the persist flag on).
   */
  clear() {
    this.stackTraces.clear();
    this.pendingRequests.clear();
    this.lazyRequestData.clear();
    this.stackTraceRequestInfoByActorID.clear();
    this._requestDataEnabled = false;
    this._lastRequestDataClearId++;
  }

  destroy() {
    // TODO: clear() is called in the middle of the lifecycle of the
    // FirefoxDataProvider, for clarity we are exposing it as a separate method.
    // `destroy` should be updated to nullify relevant instance properties.
    this.clear();
  }

  /**
   * Enable/disable firing redux actions (enabled by default).
   *
   * @param {boolean} enable Set to true to fire actions.
   */
  enableActions(enable) {
    this.actionsEnabled = enable;
  }

  /**
   * Add a new network request to application state.
   *
   * @param {string} id request id
   * @param {object} resource resource payload will be added to application state
   */
  async addRequest(id, resource) {
    // Add to the pending requests which helps when deciding if the request is complete.
    this.pendingRequests.add(id);

    if (this.actionsEnabled && this.actions.addRequest) {
      await this.actions.addRequest(id, resource, true);
    }

    this.emit(EVENTS.REQUEST_ADDED, id);
  }

  /**
   * Update a network request if it already exists in application state.
   *
   * @param {string} id request id
   * @param {object} data data payload will be updated to application state
   */
  async updateRequest(id, data) {
    const {
      responseContent,
      responseCookies,
      responseHeaders,
      requestCookies,
      requestHeaders,
      requestPostData,
      responseCache,
    } = data;

    // fetch request detail contents in parallel
    const [
      responseContentObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj,
      responseCacheObj,
    ] = await Promise.all([
      this.fetchResponseContent(responseContent),
      this.fetchRequestHeaders(requestHeaders),
      this.fetchResponseHeaders(responseHeaders),
      this.fetchPostData(requestPostData),
      this.fetchRequestCookies(requestCookies),
      this.fetchResponseCookies(responseCookies),
      this.fetchResponseCache(responseCache),
    ]);

    const payload = Object.assign(
      {},
      data,
      responseContentObj,
      requestHeadersObj,
      responseHeadersObj,
      postDataObj,
      requestCookiesObj,
      responseCookiesObj,
      responseCacheObj
    );

    if (this.actionsEnabled && this.actions.updateRequest) {
      await this.actions.updateRequest(id, payload, true);
    }

    return payload;
  }

  async fetchResponseContent(responseContent) {
    const payload = {};
    if (responseContent?.content) {
      const { text } = responseContent.content;
      const response = await this.getLongString(text);
      responseContent.content.text = response;
      payload.responseContent = responseContent;
    }
    return payload;
  }

  async fetchRequestHeaders(requestHeaders) {
    const payload = {};
    if (requestHeaders?.headers?.length) {
      const headers = await fetchHeaders(requestHeaders, this.getLongString);
      if (headers) {
        payload.requestHeaders = headers;
      }
    }
    return payload;
  }

  async fetchResponseHeaders(responseHeaders) {
    const payload = {};
    if (responseHeaders?.headers?.length) {
      const headers = await fetchHeaders(responseHeaders, this.getLongString);
      if (headers) {
        payload.responseHeaders = headers;
      }
    }
    return payload;
  }

  async fetchPostData(requestPostData) {
    const payload = {};
    if (requestPostData?.postData) {
      const { text } = requestPostData.postData;
      const postData = await this.getLongString(text);
      const headers = CurlUtils.getHeadersFromMultipartText(postData);

      // Calculate total header size and don't forget to include
      // two new-line characters at the end.
      const headersSize = headers.reduce((acc, { name, value }) => {
        return acc + name.length + value.length + 2;
      }, 0);

      requestPostData.postData.text = postData;
      payload.requestPostData = {
        ...requestPostData,
        uploadHeaders: { headers, headersSize },
      };
    }
    return payload;
  }

  async fetchRequestCookies(requestCookies) {
    const payload = {};
    if (requestCookies) {
      const reqCookies = [];
      // request store cookies in requestCookies or requestCookies.cookies
      const cookies = requestCookies.cookies
        ? requestCookies.cookies
        : requestCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (const cookie of cookies) {
          reqCookies.push(
            Object.assign({}, cookie, {
              value: await this.getLongString(cookie.value),
            })
          );
        }
        if (reqCookies.length) {
          payload.requestCookies = reqCookies;
        }
      }
    }
    return payload;
  }

  async fetchResponseCookies(responseCookies) {
    const payload = {};
    if (responseCookies) {
      const resCookies = [];
      // response store cookies in responseCookies or responseCookies.cookies
      const cookies = responseCookies.cookies
        ? responseCookies.cookies
        : responseCookies;
      // make sure cookies is iterable
      if (typeof cookies[Symbol.iterator] === "function") {
        for (const cookie of cookies) {
          resCookies.push(
            Object.assign({}, cookie, {
              value: await this.getLongString(cookie.value),
            })
          );
        }
        if (resCookies.length) {
          payload.responseCookies = resCookies;
        }
      }
    }
    return payload;
  }

  async fetchResponseCache(responseCache) {
    const payload = {};
    if (responseCache) {
      payload.responseCache = await responseCache;
      payload.responseCacheAvailable = false;
    }
    return payload;
  }

  /**
   * Public API used by the Toolbox: Tells if there is still any pending request.
   *
   * @return {boolean} returns true if pending requests still exist in the queue.
   */
  hasPendingRequests() {
    return this.pendingRequests.size > 0;
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
  async getLongString(stringGrip) {
    const payload = await getLongStringFullText(
      this.commands.client,
      stringGrip
    );
    this.emitForTests(TEST_EVENTS.LONGSTRING_RESOLVED, { payload });
    return payload;
  }

  /**
   * Retrieve the stack-trace information for the given StackTracesActor.
   *
   * @param object actor
   *        - {Object} targetFront: the target front.
   *
   *        - {String} resourceId: the resource id for the network request".
   * @return {object}
   */
  async _getStackTraceFromWatcher(actor) {
    // If we request the stack trace for the navigation request,
    // t was coming from previous page content process, which may no longer be around.
    // In any case, the previous target is destroyed and we can't fetch the stack anymore.
    let stacktrace = [];
    if (!actor.targetFront.isDestroyed()) {
      const networkContentFront = await actor.targetFront.getFront(
        "networkContent"
      );
      stacktrace = await networkContentFront.getStackTrace(
        actor.stacktraceResourceId
      );
    }
    return { stacktrace };
  }

  /**
   * The handler for when the network event stacktrace resource is available.
   * The resource contains basic info, the actual stacktrace is fetched lazily
   * using requestData.
   * @param {object} resource The network event stacktrace resource
   */
  async onStackTraceAvailable(resource) {
    if (!this.stackTraces.has(resource.resourceId)) {
      // If no stacktrace info exists, this means the network event
      // has not fired yet, lets store useful info for the NETWORK_EVENT
      // resource.
      this.stackTraces.set(resource.resourceId, resource);
    } else {
      // If stacktrace info exists, this means the network event has already
      // fired, so lets just update the reducer with the necessary stacktrace info.
      const request = this.stackTraces.get(resource.resourceId);
      request.cause.stacktraceAvailable = resource.stacktraceAvailable;
      request.cause.lastFrame = resource.lastFrame;

      this.stackTraces.delete(resource.resourceId);

      this.stackTraceRequestInfoByActorID.set(request.actor, {
        targetFront: resource.targetFront,
        stacktraceResourceId: resource.resourceId,
      });

      if (this.actionsEnabled && this.actions.updateRequest) {
        await this.actions.updateRequest(request.actor, request, true);
      }
    }
  }

  /**
   * The handler for when the network event resource is available.
   *
   * @param {object} resource The network event resource
   */
  async onNetworkResourceAvailable(resource) {
    const { actor, stacktraceResourceId, cause } = resource;

    if (!this._requestDataEnabled) {
      this._requestDataEnabled = true;
    }

    // Check if a stacktrace resource already exists for this network resource.
    if (this.stackTraces.has(stacktraceResourceId)) {
      const {
        stacktraceAvailable,
        lastFrame,
        targetFront,
      } = this.stackTraces.get(stacktraceResourceId);

      resource.cause.stacktraceAvailable = stacktraceAvailable;
      resource.cause.lastFrame = lastFrame;

      this.stackTraces.delete(stacktraceResourceId);
      // We retrieve preliminary information about the stacktrace from the
      // NETWORK_EVENT_STACKTRACE resource via `this.stackTraces` Map,
      // The actual stacktrace is fetched lazily based on the actor id, using
      // the targetFront and the stacktrace resource id therefore we
      // map these for easy access.
      this.stackTraceRequestInfoByActorID.set(actor, {
        targetFront,
        stacktraceResourceId,
      });
    } else if (cause) {
      // If the stacktrace for this request is not available, and we
      // expect that this request should have a stacktrace, lets store
      // some useful info for when the NETWORK_EVENT_STACKTRACE resource
      // finally comes.
      this.stackTraces.set(stacktraceResourceId, { actor, cause });
    }
    await this.addRequest(actor, resource);
    this.emitForTests(TEST_EVENTS.NETWORK_EVENT, resource);
  }

  /**
   * The handler for when the network event resource is updated.
   *
   * @param {object} resource The updated network event resource.
   */
  async onNetworkResourceUpdated(resource) {
    // Identify the channel as SSE if mimeType is event-stream.
    if (resource?.mimeType?.includes("text/event-stream")) {
      await this.setEventStreamFlag(resource.actor);
    }

    this.pendingRequests.delete(resource.actor);
    if (this.actionsEnabled && this.actions.updateRequest) {
      await this.actions.updateRequest(resource.actor, resource, true);
    }

    // This event is fired only once per request, once all the properties are fetched
    // from `onNetworkResourceUpdated`. There should be no more RDP requests after this.
    // Note that this event might be consumed by extension so, emit it in production
    // release as well.
    this.emitForTests(TEST_EVENTS.NETWORK_EVENT_UPDATED, resource.actor);
    this.emit(EVENTS.PAYLOAD_READY, resource);
  }

  /**
   * The "webSocketOpened" message type handler.
   *
   * @param {number} httpChannelId the channel ID
   * @param {string} effectiveURI the effective URI of the page
   * @param {string} protocols webSocket protocols
   * @param {string} extensions
   */
  async onWebSocketOpened(httpChannelId, effectiveURI, protocols, extensions) {}

  /**
   * The "webSocketClosed" message type handler.
   *
   * @param {number} httpChannelId
   * @param {boolean} wasClean
   * @param {number} code
   * @param {string} reason
   */
  async onWebSocketClosed(httpChannelId, wasClean, code, reason) {
    if (this.actionsEnabled && this.actions.closeConnection) {
      await this.actions.closeConnection(httpChannelId, wasClean, code, reason);
    }
  }

  /**
   * The "frameSent" message type handler.
   *
   * @param {number} httpChannelId the channel ID
   * @param {object} data websocket frame information
   */
  async onFrameSent(httpChannelId, data) {
    this.addMessage(httpChannelId, data);
  }

  /**
   * The "frameReceived" message type handler.
   *
   * @param {number} httpChannelId the channel ID
   * @param {object} data websocket frame information
   */
  async onFrameReceived(httpChannelId, data) {
    this.addMessage(httpChannelId, data);
  }

  /**
   * Add a new WebSocket frame to application state.
   *
   * @param {number} httpChannelId the channel ID
   * @param {object} data websocket frame information
   */
  async addMessage(httpChannelId, data) {
    if (this.actionsEnabled && this.actions.addMessage) {
      await this.actions.addMessage(httpChannelId, data, true);
    }
    // TODO: Emit an event for test here
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
    // if this is `false`, do not try to request data as requests on the backend
    // might no longer exist (usually `false` after requests are cleared).
    if (!this._requestDataEnabled) {
      return Promise.resolve();
    }
    // Key string used in `lazyRequestData`. We use this Map to prevent requesting
    // the same data twice at the same time.
    const key = `${actor}-${method}`;
    let promise = this.lazyRequestData.get(key);
    // If a request is pending, reuse it.
    if (promise) {
      return promise;
    }
    // Fetch the data
    promise = this._requestData(actor, method).then(async payload => {
      // Remove the request from the cache, any new call to requestData will fetch the
      // data again.
      this.lazyRequestData.delete(key);

      if (this.actionsEnabled && this.actions.updateRequest) {
        await this.actions.updateRequest(
          actor,
          {
            ...payload,
            // Lockdown *Available property once we fetch data from back-end.
            // Using this as a flag to prevent fetching arrived data again.
            [`${method}Available`]: false,
          },
          true
        );
      }

      return payload;
    });

    this.lazyRequestData.set(key, promise);

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
    // Backup the lastRequestDataClearId before doing any async processing.
    const lastRequestDataClearId = this._lastRequestDataClearId;

    // Calculate real name of the client getter.
    const clientMethodName = `get${method
      .charAt(0)
      .toUpperCase()}${method.slice(1)}`;
    // The name of the callback that processes request response
    const callbackMethodName = `on${method
      .charAt(0)
      .toUpperCase()}${method.slice(1)}`;
    // And the event to fire before updating this data
    const updatingEventName = `UPDATING_${method
      .replace(/([A-Z])/g, "_$1")
      .toUpperCase()}`;

    // Emit event that tell we just start fetching some data
    this.emitForTests(EVENTS[updatingEventName], actor);

    // Make sure we fetch the real actor data instead of cloned actor
    // e.g. CustomRequestPanel will clone a request with additional '-clone' actor id
    const actorID = actor.replace("-clone", "");

    // 'getStackTrace' is the only one to be fetched via the NetworkContent actor in content process
    // while all other attributes are fetched from the NetworkEvent actors, running in the parent process
    let response;
    if (
      clientMethodName == "getStackTrace" &&
      this.commands.resourceCommand.hasResourceCommandSupport(
        this.commands.resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE
      )
    ) {
      const requestInfo = this.stackTraceRequestInfoByActorID.get(actorID);
      const { stacktrace } = await this._getStackTraceFromWatcher(requestInfo);
      this.stackTraceRequestInfoByActorID.delete(actorID);
      response = { from: actor, stacktrace };
    } else {
      // We don't create fronts for NetworkEvent actors,
      // so that we have to do the request manually via DevToolsClient.request()
      try {
        const packet = {
          to: actorID,
          type: clientMethodName,
        };
        response = await this.commands.client.request(packet);
      } catch (e) {
        if (this._lastRequestDataClearId !== lastRequestDataClearId) {
          // If lastRequestDataClearId was updated, FirefoxDataProvider:clear()
          // was called and all network event actors have been destroyed.
          // Swallow errors to avoid unhandled promise rejections in tests.
          console.warn(
            `Firefox Data Provider destroyed while requesting data: ${e.message}`
          );
          // Return an empty response packet to avoid too many callback errors.
          response = { from: actor };
        } else {
          throw new Error(
            `Error while calling method ${clientMethodName}: ${e.message}`
          );
        }
      }
    }

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
  async onRequestHeaders(response) {
    const payload = await this.updateRequest(response.from, {
      requestHeaders: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_REQUEST_HEADERS, response);
    return payload.requestHeaders;
  }

  /**
   * Handles additional information received for a "responseHeaders" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onResponseHeaders(response) {
    const payload = await this.updateRequest(response.from, {
      responseHeaders: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_RESPONSE_HEADERS, response);
    return payload.responseHeaders;
  }

  /**
   * Handles additional information received for a "requestCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onRequestCookies(response) {
    const payload = await this.updateRequest(response.from, {
      requestCookies: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_REQUEST_COOKIES, response);
    return payload.requestCookies;
  }

  /**
   * Handles additional information received for a "requestPostData" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onRequestPostData(response) {
    const payload = await this.updateRequest(response.from, {
      requestPostData: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_REQUEST_POST_DATA, response);
    return payload.requestPostData;
  }

  /**
   * Handles additional information received for a "securityInfo" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onSecurityInfo(response) {
    const payload = await this.updateRequest(response.from, {
      securityInfo: response.securityInfo,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_SECURITY_INFO, response);
    return payload.securityInfo;
  }

  /**
   * Handles additional information received for a "responseCookies" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onResponseCookies(response) {
    const payload = await this.updateRequest(response.from, {
      responseCookies: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_RESPONSE_COOKIES, response);
    return payload.responseCookies;
  }

  /**
   * Handles additional information received for a "responseCache" packet.
   * @param {object} response the message received from the server.
   */
  async onResponseCache(response) {
    const payload = await this.updateRequest(response.from, {
      responseCache: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_RESPONSE_CACHE, response);
    return payload.responseCache;
  }

  /**
   * Handles additional information received via "getResponseContent" request.
   *
   * @param {object} response the message received from the server.
   */
  async onResponseContent(response) {
    const payload = await this.updateRequest(response.from, {
      // We have to ensure passing mimeType as fetchResponseContent needs it from
      // updateRequest. It will convert the LongString in `response.content.text` to a
      // string.
      mimeType: response.content.mimeType,
      responseContent: response,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_RESPONSE_CONTENT, response);
    return payload.responseContent;
  }

  /**
   * Handles additional information received for a "eventTimings" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onEventTimings(response) {
    const payload = await this.updateRequest(response.from, {
      eventTimings: response,
    });

    // This event is utilized only in tests but, DAMP is using it too
    // and running DAMP test doesn't set the `devtools.testing` flag.
    // So, emit this event even in the production mode.
    // See also: https://bugzilla.mozilla.org/show_bug.cgi?id=1578215
    this.emit(EVENTS.RECEIVED_EVENT_TIMINGS, response);
    return payload.eventTimings;
  }

  /**
   * Handles information received for a "stackTrace" packet.
   *
   * @param {object} response the message received from the server.
   */
  async onStackTrace(response) {
    const payload = await this.updateRequest(response.from, {
      stacktrace: response.stacktrace,
    });
    this.emitForTests(TEST_EVENTS.RECEIVED_EVENT_STACKTRACE, response);
    return payload.stacktrace;
  }

  /**
   * Handle EventSource events.
   */

  async onEventSourceConnectionClosed(httpChannelId) {
    if (this.actionsEnabled && this.actions.closeConnection) {
      await this.actions.closeConnection(httpChannelId);
    }
  }

  async onEventReceived(httpChannelId, data) {
    // Dispatch the same action used by websocket inspector.
    this.addMessage(httpChannelId, data);
  }

  async setEventStreamFlag(actorId) {
    if (this.actionsEnabled && this.actions.setEventStreamFlag) {
      await this.actions.setEventStreamFlag(actorId, true);
    }
  }

  /**
   * Fire events for the owner object.
   */
  emit(type, data) {
    if (this.owner) {
      this.owner.emit(type, data);
    }
  }

  /**
   * Fire test events for the owner object. These events are
   * emitted only when tests are running.
   */
  emitForTests(type, data) {
    if (this.owner) {
      this.owner.emitForTests(type, data);
    }
  }
}

module.exports = FirefoxDataProvider;
