/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {DebuggerClient} = require("./debugger-client");

function AddonClient(client, actor) {
  this._client = client;
  this._actor = actor;
  this.request = this._client.request;
  this.events = [];
}

AddonClient.prototype = {
  get actor() {
    return this._actor;
  },
  get _transport() {
    return this._client._transport;
  },

  /**
   * Detach the client from the addon actor.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (response) {
      if (this._client.activeAddon === this) {
        this._client.activeAddon = null;
      }
      this._client.unregisterClient(this);
      return response;
    },
  })
};

module.exports = AddonClient;
