/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// This worker is spawned in the parent process the first time a middleman
// connects to a cloud based replaying process, and manages the web sockets
// which are used to connect to those remote processes. This could be done on
// the main thread, but is handled here because main thread web sockets must
// be associated with particular windows, which is not the case for the
// scope which connection.js is created in.

self.addEventListener("message", function({ data }) {
  const { type, id } = data;
  switch (type) {
    case "connect":
      try {
        doConnect(id, data.channelId, data.address);
      } catch (e) {
        dump(`doConnect error: ${e}\n`);
      }
      break;
    case "send":
      try {
        doSend(id, data.buf);
      } catch (e) {
        dump(`doSend error: ${e}\n`);
      }
      break;
    default:
      ThrowError(`Unknown event type ${type}`);
  }
});

const gConnections = [];

async function doConnect(id, channelId, address) {
  if (gConnections[id]) {
    ThrowError(`Duplicate connection ID ${id}`);
  }
  const connection = { outgoing: [] };
  gConnections[id] = connection;

  // The cloud server address we are given provides us with the address of a
  // websocket server we should connect to.
  const response = await fetch(address);
  const text = await response.text();

  if (!/^wss?:\/\//.test(text)) {
    ThrowError(`Invalid websocket address ${text}`);
  }

  const socket = new WebSocket(text);
  socket.onopen = evt => onOpen(id, evt);
  socket.onclose = evt => onClose(id, evt);
  socket.onmessage = evt => onMessage(id, evt);
  socket.onerror = evt => onError(id, evt);

  await new Promise(resolve => (connection.openWaiter = resolve));

  while (gConnections[id]) {
    if (connection.outgoing.length) {
      const buf = connection.outgoing.shift();
      try {
        socket.send(buf);
      } catch (e) {
        ThrowError(`Send error ${e}`);
      }
    } else {
      await new Promise(resolve => (connection.sendWaiter = resolve));
    }
  }
}

function doSend(id, buf) {
  const connection = gConnections[id];
  connection.outgoing.push(buf);
  if (connection.sendWaiter) {
    connection.sendWaiter();
    connection.sendWaiter = null;
  }
}

function onOpen(id) {
  // Messages can now be sent to the socket.
  gConnections[id].openWaiter();
}

function onClose(id, evt) {
  gConnections[id] = null;
}

// Message data must be sent to the main thread in the order it was received.
// This is a bit tricky with web socket messages as they return data blobs,
// and blob.arrayBuffer() returns a promise such that multiple promises could
// be resolved out of order. Make sure this doesn't happen by using a single
// async frame to resolve the promises and forward them in order.
const gMessages = [];
let gMessageWaiter = null;
(async function processMessages() {
  while (true) {
    if (gMessages.length) {
      const { id, promise } = gMessages.shift();
      const buf = await promise;
      postMessage({ id, buf });
    } else {
      await new Promise(resolve => (gMessageWaiter = resolve));
    }
  }
})();

function onMessage(id, evt) {
  gMessages.push({ id, promise: evt.data.arrayBuffer() });
  if (gMessageWaiter) {
    gMessageWaiter();
    gMessageWaiter = null;
  }
}

function onError(id, evt) {
  ThrowError(`Socket error ${evt}`);
}

function ThrowError(msg) {
  dump(`Connection Worker Error: ${msg}\n`);
  throw new Error(msg);
}
