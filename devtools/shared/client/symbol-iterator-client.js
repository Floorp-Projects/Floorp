/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");

/**
 * A SymbolIteratorClient provides a way to access to symbols
 * of an object efficiently, slice by slice.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A SymbolIteratorActor grip returned by the protocol via
 *        BrowsingContextTargetActor.enumSymbols request.
 */
function SymbolIteratorClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}

SymbolIteratorClient.prototype = {
  get actor() {
    return this._grip.actor;
  },

  /**
   * Get the total number of symbols available in the iterator.
   */
  get count() {
    return this._grip.count;
  },

  /**
   * Get a set of following symbols.
   *
   * @param start Number
   *        The index of the first symbol to fetch.
   * @param count Number
   *        The number of symbols to fetch.
   * @param callback Function
   *        The function called when we receive the symbols.
   */
  slice: DebuggerClient.requester({
    type: "slice",
    start: arg(0),
    count: arg(1)
  }, {}),

  /**
   * Get all the symbols.
   *
   * @param callback Function
   *        The function called when we receive the symbols.
   */
  all: DebuggerClient.requester({
    type: "all"
  }, {}),
};

module.exports = SymbolIteratorClient;
