/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");

/**
 * Environment clients are used to manipulate the lexical environment actors.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
function EnvironmentClient(client, form) {
  this._client = client;
  this._form = form;
  this.request = this._client.request;
}
exports.EnvironmentClient = EnvironmentClient;

EnvironmentClient.prototype = {

  get actor() {
    return this._form.actor;
  },
  get _transport() {
    return this._client._transport;
  },

  /**
   * Fetches the bindings introduced by this lexical environment.
   */
  getBindings: DebuggerClient.requester({
    type: "bindings",
  }),

  /**
   * Changes the value of the identifier whose name is name (a string) to that
   * represented by value (a grip).
   */
  assign: DebuggerClient.requester({
    type: "assign",
    name: arg(0),
    value: arg(1),
  }),
};

module.exports = EnvironmentClient;
