/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("./debugger-client");

/**
 * A ArrayBufferClient provides a way to access ArrayBuffer from the
 * debugger server.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A pause-lifetime ArrayBuffer grip returned by the protocol.
 */
function ArrayBufferClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}
ArrayBufferClient.prototype = {
  get actor() {
    return this._grip.actor;
  },
  get length() {
    return this._grip.length;
  },
  get _transport() {
    return this._client._transport;
  },

  valid: true,

  slice: DebuggerClient.requester({
    type: "slice",
    start: arg(0),
    count: arg(1)
  }),
};

module.exports = ArrayBufferClient;
