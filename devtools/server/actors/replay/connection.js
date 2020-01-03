/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// This file provides an interface for connecting middleman processes with
// replaying processes living remotely in the cloud.

let gWorker;
let gCallback;
let gNextConnectionId = 1;

// eslint-disable-next-line no-unused-vars
function Initialize(callback) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", onMessage);
  gCallback = callback;
}

function onMessage(evt) {
  gCallback(evt.data.id, evt.data.buf);
}

// eslint-disable-next-line no-unused-vars
function Connect(channelId, address, callback) {
  const id = gNextConnectionId++;
  gWorker.postMessage({ type: "connect", id, channelId, address });
  return id;
}

// eslint-disable-next-line no-unused-vars
function SendMessage(id, buf) {
  gWorker.postMessage({ type: "send", id, buf });
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize", "Connect", "SendMessage"];
