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

    this.before("consoleAPICall", this.beforeConsoleAPICall);
    this.before("pageError", this.beforePageError);
  }

  get actor() {
    return this.actorID;
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
   * Close the WebConsoleFront.
   *
   */
  destroy() {
    if (!this._client) {
      return null;
    }

    // This will make future calls to this function harmless because of the early return
    // at the top of the function.
    this._client = null;

    return super.destroy();
  }
}

exports.WebConsoleFront = WebConsoleFront;
registerFront(WebConsoleFront);
