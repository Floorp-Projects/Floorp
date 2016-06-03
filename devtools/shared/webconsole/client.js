/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const {LongStringClient} = require("devtools/shared/client/main");

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
function WebConsoleClient(debuggerClient, response) {
  this._actor = response.from;
  this._client = debuggerClient;
  this._longStrings = {};
  this.traits = response.traits || {};
  this.events = [];
  this._networkRequests = new Map();

  this.pendingEvaluationResults = new Map();
  this.onEvaluationResult = this.onEvaluationResult.bind(this);
  this.onNetworkEvent = this._onNetworkEvent.bind(this);
  this.onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);

  this._client.addListener("evaluationResult", this.onEvaluationResult);
  this._client.addListener("networkEvent", this.onNetworkEvent);
  this._client.addListener("networkEventUpdate", this.onNetworkEventUpdate);
  EventEmitter.decorate(this);
}

exports.WebConsoleClient = WebConsoleClient;

WebConsoleClient.prototype = {
  _longStrings: null,
  traits: null,

  /**
   * Holds the network requests currently displayed by the Web Console. Each key
   * represents the connection ID and the value is network request information.
   * @private
   * @type object
   */
  _networkRequests: null,

  getNetworkRequest(actorId) {
    return this._networkRequests.get(actorId);
  },

  hasNetworkRequest(actorId) {
    return this._networkRequests.has(actorId);
  },

  removeNetworkRequest(actorId) {
    this._networkRequests.delete(actorId);
  },

  getNetworkEvents() {
    return this._networkRequests.values();
  },

  get actor() {
    return this._actor;
  },

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
  _onNetworkEvent: function (type, packet) {
    if (packet.from == this._actor) {
      let actor = packet.eventActor;
      let networkInfo = {
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
        fromServiceWorker: actor.fromServiceWorker
      };
      this._networkRequests.set(actor.actor, networkInfo);

      this.emit("networkEvent", networkInfo);
    }
  },

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
  _onNetworkEventUpdate: function (type, packet) {
    let networkInfo = this.getNetworkRequest(packet.from);
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
        networkInfo.securityInfo = packet.state;
        break;
    }

    this.emit("networkEventUpdate", {
      packet: packet,
      networkInfo
    });
  },

  /**
   * Retrieve the cached messages from the server.
   *
   * @see this.CACHED_MESSAGES
   * @param array types
   *        The array of message types you want from the server. See
   *        this.CACHED_MESSAGES for known types.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getCachedMessages: function (types, onResponse) {
    let packet = {
      to: this._actor,
      type: "getCachedMessages",
      messageTypes: types,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Inspect the properties of an object.
   *
   * @param string actor
   *        The WebConsoleObjectActor ID to send the request to.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  inspectObjectProperties: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "inspectProperties",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Evaluate a JavaScript expression.
   *
   * @param string string
   *        The code you want to evaluate.
   * @param function onResponse
   *        The function invoked when the response is received.
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
   */
  evaluateJS: function (string, onResponse, options = {}) {
    let packet = {
      to: this._actor,
      type: "evaluateJS",
      text: string,
      bindObjectActor: options.bindObjectActor,
      frameActor: options.frameActor,
      url: options.url,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Evaluate a JavaScript expression asynchronously.
   * See evaluateJS for parameter and response information.
   */
  evaluateJSAsync: function (string, onResponse, options = {}) {
    // Pre-37 servers don't support async evaluation.
    if (!this.traits.evaluateJSAsync) {
      this.evaluateJS(string, onResponse, options);
      return;
    }

    let packet = {
      to: this._actor,
      type: "evaluateJSAsync",
      text: string,
      bindObjectActor: options.bindObjectActor,
      frameActor: options.frameActor,
      url: options.url,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };

    this._client.request(packet, response => {
      // Null check this in case the client has been detached while waiting
      // for a response.
      if (this.pendingEvaluationResults) {
        this.pendingEvaluationResults.set(response.resultID, onResponse);
      }
    });
  },

  /**
   * Handler for the actors's unsolicited evaluationResult packet.
   */
  onEvaluationResult: function (notification, packet) {
    // The client on the main thread can receive notification packets from
    // multiple webconsole actors: the one on the main thread and the ones
    // on worker threads.  So make sure we should be handling this request.
    if (packet.from !== this._actor) {
      return;
    }

    // Find the associated callback based on this ID, and fire it.
    // In a sync evaluation, this would have already been called in
    // direct response to the client.request function.
    let onResponse = this.pendingEvaluationResults.get(packet.resultID);
    if (onResponse) {
      onResponse(packet);
      this.pendingEvaluationResults.delete(packet.resultID);
    } else {
      DevToolsUtils.reportException("onEvaluationResult",
        "No response handler for an evaluateJSAsync result (resultID: " +
                                    packet.resultID + ")");
    }
  },

  /**
   * Autocomplete a JavaScript expression.
   *
   * @param string string
   *        The code you want to autocomplete.
   * @param number cursor
   *        Cursor location inside the string. Index starts from 0.
   * @param function onResponse
   *        The function invoked when the response is received.
   * @param string frameActor
   *        The id of the frame actor that made the call.
   */
  autocomplete: function (string, cursor, onResponse, frameActor) {
    let packet = {
      to: this._actor,
      type: "autocomplete",
      text: string,
      cursor: cursor,
      frameActor: frameActor,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Clear the cache of messages (page errors and console API calls).
   */
  clearMessagesCache: function () {
    let packet = {
      to: this._actor,
      type: "clearMessagesCache",
    };
    this._client.request(packet);
  },

  /**
   * Get Web Console-related preferences on the server.
   *
   * @param array preferences
   *        An array with the preferences you want to retrieve.
   * @param function [onResponse]
   *        Optional function to invoke when the response is received.
   */
  getPreferences: function (preferences, onResponse) {
    let packet = {
      to: this._actor,
      type: "getPreferences",
      preferences: preferences,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Set Web Console-related preferences on the server.
   *
   * @param object preferences
   *        An object with the preferences you want to change.
   * @param function [onResponse]
   *        Optional function to invoke when the response is received.
   */
  setPreferences: function (preferences, onResponse) {
    let packet = {
      to: this._actor,
      type: "setPreferences",
      preferences: preferences,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the request headers from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getRequestHeaders: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getRequestHeaders",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the request cookies from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getRequestCookies: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getRequestCookies",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the request post data from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getRequestPostData: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getRequestPostData",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the response headers from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getResponseHeaders: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getResponseHeaders",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the response cookies from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getResponseCookies: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getResponseCookies",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the response content from the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getResponseContent: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getResponseContent",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the timing information for the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getEventTimings: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getEventTimings",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Retrieve the security information for the given NetworkEventActor.
   *
   * @param string actor
   *        The NetworkEventActor ID.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  getSecurityInfo: function (actor, onResponse) {
    let packet = {
      to: actor,
      type: "getSecurityInfo",
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Send a HTTP request with the given data.
   *
   * @param string data
   *        The details of the HTTP request.
   * @param function onResponse
   *        The function invoked when the response is received.
   */
  sendHTTPRequest: function (data, onResponse) {
    let packet = {
      to: this._actor,
      type: "sendHTTPRequest",
      request: data
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Start the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array listeners
   *        Array of listeners you want to start. See this.LISTENERS for
   *        known listeners.
   * @param function onResponse
   *        Function to invoke when the server response is received.
   */
  startListeners: function (listeners, onResponse) {
    let packet = {
      to: this._actor,
      type: "startListeners",
      listeners: listeners,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Stop the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array listeners
   *        Array of listeners you want to stop. See this.LISTENERS for
   *        known listeners.
   * @param function onResponse
   *        Function to invoke when the server response is received.
   */
  stopListeners: function (listeners, onResponse) {
    let packet = {
      to: this._actor,
      type: "stopListeners",
      listeners: listeners,
    };
    this._client.request(packet, onResponse);
  },

  /**
   * Return an instance of LongStringClient for the given long string grip.
   *
   * @param object grip
   *        The long string grip returned by the protocol.
   * @return object
   *         The LongStringClient for the given long string grip.
   */
  longString: function (grip) {
    if (grip.actor in this._longStrings) {
      return this._longStrings[grip.actor];
    }

    let client = new LongStringClient(this._client, grip);
    this._longStrings[grip.actor] = client;
    return client;
  },

  /**
   * Close the WebConsoleClient. This stops all the listeners on the server and
   * detaches from the console actor.
   *
   * @param function onResponse
   *        Function to invoke when the server response is received.
   */
  detach: function (onResponse) {
    this._client.removeListener("evaluationResult", this.onEvaluationResult);
    this._client.removeListener("networkEvent", this.onNetworkEvent);
    this._client.removeListener("networkEventUpdate",
                                this.onNetworkEventUpdate);
    this.stopListeners(null, onResponse);
    this._longStrings = null;
    this._client = null;
    this.pendingEvaluationResults.clear();
    this.pendingEvaluationResults = null;
    this.clearNetworkRequests();
    this._networkRequests = null;
  },

  clearNetworkRequests: function () {
    this._networkRequests.clear();
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
    // Make sure this is a long string.
    if (typeof stringGrip != "object" || stringGrip.type != "longString") {
      // Go home string, you're drunk.
      return promise.resolve(stringGrip);
    }

    // Fetch the long string only once.
    if (stringGrip._fullText) {
      return stringGrip._fullText.promise;
    }

    let deferred = stringGrip._fullText = promise.defer();
    let { initial, length } = stringGrip;
    let longStringClient = this.longString(stringGrip);

    longStringClient.substring(initial.length, length, response => {
      if (response.error) {
        DevToolsUtils.reportException("getString",
            response.error + ": " + response.message);

        deferred.reject(response);
        return;
      }
      deferred.resolve(initial + response.substring);
    });

    return deferred.promise;
  }
};
