/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { LongStringFront } = require("devtools/client/fronts/string");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { webconsoleSpec } = require("devtools/shared/specs/webconsole");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/client/fronts/object");

/**
 * A WebConsoleFront is used as a front end for the WebConsoleActor that is
 * created on the server, hiding implementation details.
 *
 * @param object client
 *        The DevToolsClient instance we live for.
 */
class WebConsoleFront extends FrontClassWithSpec(webconsoleSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;
    this.traits = {};
    this._longStrings = {};
    this.events = [];

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "consoleActor";

    this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);

    this.before("consoleAPICall", this.beforeConsoleAPICall);
    this.before("pageError", this.beforePageError);
    this.before("serverNetworkEvent", this.beforeServerNetworkEvent);

    this._client.on("networkEventUpdate", this._onNetworkEventUpdate);
  }

  get actor() {
    return this.actorID;
  }

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onNetworkEventUpdate(packet) {
    this.emit("serverNetworkUpdateEvent", packet);
  }

  beforeServerNetworkEvent(packet) {
    // The stacktrace info needs to be sent before
    // the network event.
    this.emit("serverNetworkStackTrace", packet);
  }

  beforeConsoleAPICall(packet) {
    if (packet.message && Array.isArray(packet.message.arguments)) {
      // We might need to create fronts for each of the message arguments.
      packet.message.arguments = packet.message.arguments.map(arg =>
        getAdHocFrontOrPrimitiveGrip(arg, this)
      );
    }
    return packet;
  }

  beforePageError(packet) {
    if (packet?.pageError?.errorMessage) {
      packet.pageError.errorMessage = getAdHocFrontOrPrimitiveGrip(
        packet.pageError.errorMessage,
        this
      );
    }

    if (packet?.pageError?.exception) {
      packet.pageError.exception = getAdHocFrontOrPrimitiveGrip(
        packet.pageError.exception,
        this
      );
    }
    return packet;
  }

  async getCachedMessages(messageTypes) {
    const response = await super.getCachedMessages(messageTypes);
    if (Array.isArray(response.messages)) {
      response.messages = response.messages.map(packet => {
        if (Array.isArray(packet?.message?.arguments)) {
          // We might need to create fronts for each of the message arguments.
          packet.message.arguments = packet.message.arguments.map(arg =>
            getAdHocFrontOrPrimitiveGrip(arg, this)
          );
        }

        if (packet.pageError?.exception) {
          packet.pageError.exception = getAdHocFrontOrPrimitiveGrip(
            packet.pageError.exception,
            this
          );
        }

        return packet;
      });
    }
    return response;
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
   * Start the given Web Console listeners.
   * TODO: remove once the front is retrieved via getFront, and we use form()
   *
   * @see this.LISTENERS
   * @param array listeners
   *        Array of listeners you want to start. See this.LISTENERS for
   *        known listeners.
   * @return request
   *         Request object that implements both Promise and EventEmitter interfaces
   */
  async startListeners(listeners) {
    const response = await super.startListeners(listeners);
    this.hasNativeConsoleAPI = response.nativeConsoleAPI;
    this.traits = response.traits;
    return response;
  }

  /**
   * Return an instance of LongStringFront for the given long string grip.
   *
   * @param object grip
   *        The long string grip returned by the protocol.
   * @return {LongStringFront} the front for the given long string grip.
   */
  longString(grip) {
    if (grip.actor in this._longStrings) {
      return this._longStrings[grip.actor];
    }

    const front = new LongStringFront(this._client, this.targetFront, this);
    front.form(grip);
    this.manage(front);
    this._longStrings[grip.actor] = front;
    return front;
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
  async getString(stringGrip) {
    // Make sure this is a long string.
    if (typeof stringGrip !== "object" || stringGrip.type !== "longString") {
      // Go home string, you're drunk.
      return stringGrip;
    }

    // Fetch the long string only once.
    if (stringGrip._fullText) {
      return stringGrip._fullText;
    }

    const { initial, length } = stringGrip;
    const longStringFront = this.longString(stringGrip);

    try {
      const response = await longStringFront.substring(initial.length, length);
      return initial + response;
    } catch (e) {
      DevToolsUtils.reportException("getString", e.message);
      throw e;
    }
  }

  /**
   * Close the WebConsoleFront.
   *
   */
  destroy() {
    if (!this._client) {
      return null;
    }

    this._client.off("networkEventUpdate", this._onNetworkEventUpdate);
    // This will make future calls to this function harmless because of the early return
    // at the top of the function.
    this._client = null;

    this._longStrings = null;
    return super.destroy();
  }
}

exports.WebConsoleFront = WebConsoleFront;
registerFront(WebConsoleFront);
