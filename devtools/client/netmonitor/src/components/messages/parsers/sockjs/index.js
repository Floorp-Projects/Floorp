/*
 * SockJS is a browser JavaScript library that provides a WebSocket-like object.
 * SockJS gives you a coherent, cross-browser, Javascript API which creates a low latency,
 * full duplex, cross-domain communication channel between the browser and the web server.
 *
 * Copyright (c) 2011-2018 The sockjs-client Authors.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of the library source tree.
 *
 * https://github.com/sockjs/sockjs-client
 */

"use strict";

function parseSockJS(msg) {
  const type = msg.slice(0, 1);
  const content = msg.slice(1);

  // first check for messages that don't need a payload
  switch (type) {
    case "o":
      return { type: "open" };
    case "h":
      return { type: "heartbeat" };
  }

  let payload;
  if (content) {
    try {
      payload = JSON.parse(content);
    } catch (e) {
      return null;
    }
  }

  if (typeof payload === "undefined") {
    return null;
  }

  switch (type) {
    case "a":
      return { type: "message", data: payload };
    case "m":
      return { type: "message", data: payload };
    case "c":
      const [code, message] = payload;
      return { type: "close", code, message };
    default:
      return null;
  }
}

module.exports = {
  parseSockJS,
};
