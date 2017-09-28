/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("./debugger-client");
/**
 * A LongStringClient provides a way to access "very long" strings from the
 * debugger server.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A pause-lifetime long string grip returned by the protocol.
 */
function LongStringClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}

LongStringClient.prototype = {
  get actor() {
    return this._grip.actor;
  },
  get length() {
    return this._grip.length;
  },
  get initial() {
    return this._grip.initial;
  },
  get _transport() {
    return this._client._transport;
  },

  valid: true,

  /**
   * Get the substring of this LongString from start to end.
   *
   * @param start Number
   *        The starting index.
   * @param end Number
   *        The ending index.
   * @param callback Function
   *        The function called when we receive the substring.
   */
  substring: DebuggerClient.requester({
    type: "substring",
    start: arg(0),
    end: arg(1)
  }),
};

module.exports = LongStringClient;
