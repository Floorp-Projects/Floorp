/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const LongStringClient = require("devtools/shared/client/long-string-client");
const { FrontClassWithSpec } = require("devtools/shared/protocol");
const { webconsoleSpec } = require("devtools/shared/specs/webconsole");

/**
 * A WebConsoleClient is used as a front end for the WebConsoleActor that is
 * created on the server, hiding implementation details.
 *
 * @param object debuggerClient
 *        The DebuggerClient instance we live for.
 * @param object response
 *        The response packet received from the "startListeners" request sent to
 *        the WebConsoleActor.
 */
class WebConsoleClient extends FrontClassWithSpec(webconsoleSpec) {
  constructor(client, form) {
    super(client);
    this.actorID = form.from;
    this._client = client;
    this.traits = form.traits || {};
    this._longStrings = {};
    this.events = [];

    /**
     * Holds the network requests currently displayed by the Web Console. Each key
     * represents the connection ID and the value is network request information.
     * @private
     * @type object
     */
    this._networkRequests = new Map();

    this.pendingEvaluationResults = new Map();
    this.onEvaluationResult = this.onEvaluationResult.bind(this);
    this.onNetworkEvent = this._onNetworkEvent.bind(this);
    this.onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
    this.onInspectObject = this._onInspectObject.bind(this);
    this.onDocEvent = this._onDocEvent.bind(this);

    this._client.addListener("evaluationResult", this.onEvaluationResult);
    this._client.addListener("networkEvent", this.onNetworkEvent);
    this._client.addListener("networkEventUpdate", this.onNetworkEventUpdate);
    this._client.addListener("inspectObject", this.onInspectObject);
    this._client.addListener("documentEvent", this.onDocEvent);
    this.manage(this);
  }

  getNetworkRequest(actorId) {
    return this._networkRequests.get(actorId);
  }

  getNetworkEvents() {
    return this._networkRequests.values();
  }

  get actor() {
    return this.actorID;
  }

  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onNetworkEvent(type, packet) {
    if (packet.from == this.actorID) {
      const actor = packet.eventActor;
      const networkInfo = {
        _type: "NetworkEvent",
        timeStamp: actor.timeStamp,
        node: null,
        actor: actor.actor,
        discardRequestBody: true,
        discardResponseBody: true,
        startedDateTime: actor.startedDateTime,
        request: {
          url: actor.url,
          method: actor.method,
        },
        isXHR: actor.isXHR,
        cause: actor.cause,
        response: {},
        timings: {},
        // track the list of network event updates
        updates: [],
        private: actor.private,
        fromCache: actor.fromCache,
        fromServiceWorker: actor.fromServiceWorker,
        isThirdPartyTrackingResource: actor.isThirdPartyTrackingResource,
        referrerPolicy: actor.referrerPolicy,
      };
      this._networkRequests.set(actor.actor, networkInfo);

      this.emit("networkEvent", networkInfo);
    }
  }

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onNetworkEventUpdate(type, packet) {
    const networkInfo = this.getNetworkRequest(packet.from);
    if (!networkInfo) {
      return;
    }

    networkInfo.updates.push(packet.updateType);

    switch (packet.updateType) {
      case "requestHeaders":
        networkInfo.request.headersSize = packet.headersSize;
        break;
      case "requestPostData":
        networkInfo.discardRequestBody = packet.discardRequestBody;
        networkInfo.request.bodySize = packet.dataSize;
        break;
      case "responseStart":
        networkInfo.response.httpVersion = packet.response.httpVersion;
        networkInfo.response.status = packet.response.status;
        networkInfo.response.statusText = packet.response.statusText;
        networkInfo.response.headersSize = packet.response.headersSize;
        networkInfo.response.remoteAddress = packet.response.remoteAddress;
        networkInfo.response.remotePort = packet.response.remotePort;
        networkInfo.discardResponseBody = packet.response.discardResponseBody;
        break;
      case "responseContent":
        networkInfo.response.content = {
          mimeType: packet.mimeType,
        };
        networkInfo.response.bodySize = packet.contentSize;
        networkInfo.response.transferredSize = packet.transferredSize;
        networkInfo.discardResponseBody = packet.discardResponseBody;
        break;
      case "eventTimings":
        networkInfo.totalTime = packet.totalTime;
        break;
      case "securityInfo":
        networkInfo.securityState = packet.state;
        break;
      case "responseCache":
        networkInfo.response.responseCache = packet.responseCache;
        break;
    }

    this.emit("networkEventUpdate", {
      packet: packet,
      networkInfo,
    });
  }

  /**
   * The "inspectObject" message type handler. We just re-emit it so that
   * the toolbox can listen to the event and decide how to handle it.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onInspectObject(type, packet) {
    this.emit("inspectObject", packet);
  }

  /**
   * The "docEvent" message type handler. We just re-emit it so that
   * the tools can listen for them on the console client.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onDocEvent(type, packet) {
    this.emit("documentEvent", packet);
  }

  /**
   * Retrieve the cached messages from the server.
   *
   * @see this.CACHED_MESSAGES
   * @param array types
   *        The array of message types you want from the server. See
   *        this.CACHED_MESSAGES for known types.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getCachedMessages(messageTypes) {
    return super.getCachedMessages({ messageTypes });
  }

  /**
   * Evaluate a JavaScript expression.
   *
   * @param string string
   *        The code you want to evaluate.
   * @param object [options={}]
   *        Options for evaluation:
   *
   *        - bindObjectActor: an ObjectActor ID. The OA holds a reference to
   *        a Debugger.Object that wraps a content object. This option allows
   *        you to bind |_self| to the D.O of the given OA, during string
   *        evaluation.
   *
   *        See: Debugger.Object.executeInGlobalWithBindings() for information
   *        about bindings.
   *
   *        Use case: the variable view needs to update objects and it does so
   *        by knowing the ObjectActor it inspects and binding |_self| to the
   *        D.O of the OA. As such, variable view sends strings like these for
   *        eval:
   *          _self["prop"] = value;
   *
   *        - frameActor: a FrameActor ID. The FA holds a reference to
   *        a Debugger.Frame. This option allows you to evaluate the string in
   *        the frame of the given FA.
   *
   *        - url: the url to evaluate the script as. Defaults to
   *        "debugger eval code".
   *
   *        - selectedNodeActor: the NodeActor ID of the current
   *        selection in the Inspector, if such a selection
   *        exists. This is used by helper functions that can
   *        reference the currently selected node in the Inspector,
   *        like $0.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  evaluateJS(string, opts = {}) {
    const options = {
      text: string,
      bindObjectActor: opts.bindObjectActor,
      frameActor: opts.frameActor,
      url: opts.url,
      selectedNodeActor: opts.selectedNodeActor,
      selectedObjectActor: opts.selectedObjectActor,
    };
    return super.evaluateJS(options);
  }

  /**
   * Evaluate a JavaScript expression asynchronously.
   * See evaluateJS for parameter and response information.
   */
  evaluateJSAsync(string, opts = {}) {
    const options = {
      text: string,
      bindObjectActor: opts.bindObjectActor,
      frameActor: opts.frameActor,
      url: opts.url,
      selectedNodeActor: opts.selectedNodeActor,
      selectedObjectActor: opts.selectedObjectActor,
      mapped: opts.mapped,
    };

    return new Promise(async (resolve, reject) => {
      const response = await super.evaluateJSAsync(options);
      // Null check this in case the client has been detached while waiting
      // for a response.
      if (this.pendingEvaluationResults) {
        this.pendingEvaluationResults.set(response.resultID, resp => {
          if (resp.error) {
            reject(resp);
          } else {
            resolve(resp);
          }
        });
      }
    });
  }

  /**
   * Handler for the actors's unsolicited evaluationResult packet.
   */
  onEvaluationResult(notification, packet) {
    // The client on the main thread can receive notification packets from
    // multiple webconsole actors: the one on the main thread and the ones
    // on worker threads.  So make sure we should be handling this request.
    if (packet.from !== this.actorID) {
      return;
    }

    // Find the associated callback based on this ID, and fire it.
    // In a sync evaluation, this would have already been called in
    // direct response to the client.request function.
    const onResponse = this.pendingEvaluationResults.get(packet.resultID);
    if (onResponse) {
      onResponse(packet);
      this.pendingEvaluationResults.delete(packet.resultID);
    } else {
      DevToolsUtils.reportException("onEvaluationResult",
        "No response handler for an evaluateJSAsync result (resultID: " +
                                    packet.resultID + ")");
    }
  }

  /**
   * Autocomplete a JavaScript expression.
   *
   * @param {String} string
   *        The code you want to autocomplete.
   * @param {Number} cursor
   *        Cursor location inside the string. Index starts from 0.
   * @param {String} frameActor
   *        The id of the frame actor that made the call.
   * @param {String} selectedNodeActor: Actor id of the selected node in the inspector.
   * @param {Array} authorizedEvaluations
   *        Array of the properties access which can be executed by the engine.
   *        Example: [["x", "myGetter"], ["x", "myGetter", "y", "anotherGetter"]] to
   *        retrieve properties of `x.myGetter.` and `x.myGetter.y.anotherGetter`.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  autocomplete(
    string,
    cursor,
    frameActor,
    selectedNodeActor,
    authorizedEvaluations
  ) {
    const options = {
      text: string,
      cursor,
      frameActor,
      selectedNodeActor,
      authorizedEvaluations,
    };
    return super.autocomplete(options);
  }

  /**
   * Clear the cache of messages (page errors and console API calls).
   *
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  clearMessagesCache() {
    return super.clearMessagesCache();
  }

  /**
   * Get Web Console-related preferences on the server.
   *
   * @param array preferences
   *        An array with the preferences you want to retrieve.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getPreferences(preferences) {
    return super.getPreferences({ preferences });
  }

  /**
   * Set Web Console-related preferences on the server.
   *
   * @param object preferences
   *        An object with the preferences you want to change.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  setPreferences(preferences) {
    return super.setPreferences({ preferences });
  }

  /**
   * Retrieve the request headers from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getRequestHeaders(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getRequestHeaders",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the request cookies from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getRequestCookies(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getRequestCookies",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the request post data from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getRequestPostData(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getRequestPostData",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the response headers from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getResponseHeaders(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getResponseHeaders",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the response cookies from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getResponseCookies(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getResponseCookies",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the response content from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getResponseContent(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getResponseContent",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the response cache from the given NetworkEventActor
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces.
   */
  getResponseCache(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getResponseCache",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the timing information for the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getEventTimings(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getEventTimings",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the security information for the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getSecurityInfo(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getSecurityInfo",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Retrieve the stack-trace information for the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the stack-trace is received.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  getStackTrace(actor, onResponse) {
    const packet = {
      to: actor,
      type: "getStackTrace",
    };
    return this._client.request(packet, onResponse);
  }

  /**
   * Send a HTTP request with the given data.
   *
   * @param string data
   *        The details of the HTTP request.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  sendHTTPRequest(data) {
    return super.sendHTTPRequest({ request: data });
  }

  /**
   * Start the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array listeners
   *        Array of listeners you want to start. See this.LISTENERS for
   *        known listeners.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  startListeners(listeners) {
    return super.startListeners({ listeners });
  }

  /**
   * Stop the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array listeners
   *        Array of listeners you want to stop. See this.LISTENERS for
   *        known listeners.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  stopListeners(listeners) {
    return super.stopListeners({ listeners });
  }

  /**
   * Return an instance of LongStringClient for the given long string grip.
   *
   * @param object grip
   *        The long string grip returned by the protocol.
   * @return object
   *         The LongStringClient for the given long string grip.
   */
  longString(grip) {
    if (grip.actor in this._longStrings) {
      return this._longStrings[grip.actor];
    }

    const client = new LongStringClient(this._client, grip);
    this._longStrings[grip.actor] = client;
    return client;
  }

  /**
   * Close the WebConsoleClient.
   *
   */
  destroy() {
    this._client.removeListener("evaluationResult", this.onEvaluationResult);
    this._client.removeListener("networkEvent", this.onNetworkEvent);
    this._client.removeListener("networkEventUpdate",
                                this.onNetworkEventUpdate);
    this._client.removeListener("inspectObject", this.onInspectObject);
    this._client.removeListener("documentEvent", this.onDocEvent);
    this._longStrings = null;
    this._client = null;
    this.pendingEvaluationResults.clear();
    this.pendingEvaluationResults = null;
    this.clearNetworkRequests();
    this._networkRequests = null;
    super.destroy();
  }

  clearNetworkRequests() {
    this._networkRequests.clear();
  }

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
  getString(stringGrip) {
    // Make sure this is a long string.
    if (typeof stringGrip !== "object" || stringGrip.type !== "longString") {
      // Go home string, you're drunk.
      return Promise.resolve(stringGrip);
    }

    // Fetch the long string only once.
    if (stringGrip._fullText) {
      return stringGrip._fullText;
    }

    return new Promise((resolve, reject) => {
      const { initial, length } = stringGrip;
      const longStringClient = this.longString(stringGrip);

      longStringClient.substring(initial.length, length, response => {
        if (response.error) {
          DevToolsUtils.reportException("getString",
              response.error + ": " + response.message);
          reject(response);
        }
        resolve(initial + response.substring);
      });
    });
  }
}

exports.WebConsoleClient = WebConsoleClient;
