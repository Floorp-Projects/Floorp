/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");

/**
 * Creates a tracing profiler client for the remote debugging protocol
 * server. This client is a front to the trace actor created on the
 * server side, hiding the protocol details in a traditional
 * JavaScript API.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param actor string
 *        The actor ID for this thread.
 */
function TraceClient(client, actor) {
  this._client = client;
  this._actor = actor;
  this._activeTraces = new Set();
  this._waitingPackets = new Map();
  this._expectedPacket = 0;
  this.request = this._client.request;
  this.events = [];
}

TraceClient.prototype = {
  get actor() {
    return this._actor;
  },
  get tracing() {
    return this._activeTraces.size > 0;
  },

  get _transport() {
    return this._client._transport;
  },

  /**
   * Detach from the trace actor.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (response) {
      this._client.unregisterClient(this);
      return response;
    },
  }),

  /**
   * Start a new trace.
   *
   * @param trace [string]
   *        An array of trace types to be recorded by the new trace.
   *
   * @param name string
   *        The name of the new trace.
   *
   * @param onResponse function
   *        Called with the request's response.
   */
  startTrace: DebuggerClient.requester({
    type: "startTrace",
    name: arg(1),
    trace: arg(0)
  }, {
    after: function (response) {
      if (response.error) {
        return response;
      }

      if (!this.tracing) {
        this._waitingPackets.clear();
        this._expectedPacket = 0;
      }
      this._activeTraces.add(response.name);

      return response;
    },
  }),

  /**
   * End a trace. If a name is provided, stop the named
   * trace. Otherwise, stop the most recently started trace.
   *
   * @param name string
   *        The name of the trace to stop.
   *
   * @param onResponse function
   *        Called with the request's response.
   */
  stopTrace: DebuggerClient.requester({
    type: "stopTrace",
    name: arg(0)
  }, {
    after: function (response) {
      if (response.error) {
        return response;
      }

      this._activeTraces.delete(response.name);

      return response;
    },
  })
};

module.exports = TraceClient;
