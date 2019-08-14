/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { webSocketSpec } = require("devtools/shared/specs/websocket");

/**
 * A WebSocketFront is used as a front end for the WebSocketActor that is
 * created on the server, hiding implementation details.
 */
class WebSocketFront extends FrontClassWithSpec(webSocketSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._onWebSocketOpened = this._onWebSocketOpened.bind(this);
    this._onWebSocketClosed = this._onWebSocketClosed.bind(this);
    this._onFrameSent = this._onFrameSent.bind(this);
    this._onFrameReceived = this._onFrameReceived.bind(this);

    // Attribute name from which to retrieve the actorID
    // out of the target actor's form
    this.formAttributeName = "webSocketActor";

    this.on("serverWebSocketOpened", this._onWebSocketOpened);
    this.on("serverWebSocketClosed", this._onWebSocketClosed);
    this.on("serverFrameSent", this._onFrameSent);
    this.on("serverFrameReceived", this._onFrameReceived);
  }

  /**
   * Close the WebSocketFront.
   *
   */
  destroy() {
    this.off("serverWebSocketOpened");
    this.off("serverWebSocketClosed");
    this.off("serverFrameSent");
    this.off("serverFrameReceived");
    return super.destroy();
  }

  /**
   * The "webSocketOpened" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param number httpChannelId
   *        Channel ID of the websocket connection.
   * @param string effectiveURI
   *        URI of the page.
   * @param string protocols
   *        WebSocket procotols.
   * @param string extensions
   */
  async _onWebSocketOpened(httpChannelId, effectiveURI, protocols, extensions) {
    this.emit(
      "webSocketOpened",
      httpChannelId,
      effectiveURI,
      protocols,
      extensions
    );
  }

  /**
   * The "webSocketClosed" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param boolean wasClean
   * @param number code
   * @param string reason
   */
  async _onWebSocketClosed(wasClean, code, reason) {
    this.emit("webSocketClosed", wasClean, code, reason);
  }

  /**
   * The "frameReceived" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string httpChannelId
   *        Channel ID of the websocket connection.
   * @param object data
   *        The data received from the server.
   */
  async _onFrameReceived(httpChannelId, data) {
    this.emit("frameReceived", httpChannelId, data);
  }

  /**
   * The "frameSent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string httpChannelId
   *        Channel ID of the websocket connection.
   * @param object data
   *        The data received from the server.
   */
  async _onFrameSent(httpChannelId, data) {
    this.emit("frameSent", httpChannelId, data);
  }
}

exports.WebSocketFront = WebSocketFront;
registerFront(WebSocketFront);
