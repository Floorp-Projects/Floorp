/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  webconsoleSpec,
} = require("resource://devtools/shared/specs/webconsole.js");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("resource://devtools/client/fronts/object.js");

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

    return super.destroy();
  }
}

exports.WebConsoleFront = WebConsoleFront;
registerFront(WebConsoleFront);
