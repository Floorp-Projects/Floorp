/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, generateActorSpec } = require("devtools/shared/protocol");

const webSocketSpec = generateActorSpec({
  typeName: "webSocket",

  /**
   * The set of events the WebSocketActor emits over RDP.
   */
  events: {
    // In order to avoid a naming collision, we rename the server event.
    serverWebSocketOpened: {
      type: "webSocketOpened",
      httpChannelId: Arg(0, "number"),
      effectiveURI: Arg(1, "string"),
      protocols: Arg(2, "string"),
      extensions: Arg(3, "string"),
    },
    serverWebSocketClosed: {
      type: "webSocketClosed",
      httpChannelId: Arg(0, "number"),
      wasClean: Arg(1, "boolean"),
      code: Arg(2, "number"),
      reason: Arg(3, "string"),
    },
    serverFrameReceived: {
      type: "frameReceived",
      httpChannelId: Arg(0, "number"),
      data: Arg(1, "json"),
    },
    serverFrameSent: {
      type: "frameSent",
      httpChannelId: Arg(0, "number"),
      data: Arg(1, "json"),
    },
  },

  methods: {
    startListening: {
      request: {},
      oneway: true,
    },
    stopListening: {
      request: {},
      oneway: true,
    },
  },
});

exports.webSocketSpec = webSocketSpec;
