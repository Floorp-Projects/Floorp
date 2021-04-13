/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Each worker debugger supports only a single connection to the main thread.
// However, its theoretically possible for multiple servers to connect to the
// same worker. Consequently, each transport has a connection id, to allow
// messages from multiple connections to be multiplexed on a single channel.

/**
 * A transport that uses a WorkerDebugger to send packets from the main
 * thread to a worker thread.
 */
class MainThreadWorkerDebuggerTransport {
  constructor(dbg, id) {
    this._dbg = dbg;
    this._id = id;

    this._dbgListener = {
      onMessage: this._onMessage.bind(this),
    };
  }

  ready() {
    this._dbg.addListener(this._dbgListener);
  }

  close() {
    if (this._dbgListener) {
      this._dbg.removeListener(this._dbgListener);
    }
    this._dbgListener = null;
    this.hooks?.onTransportClosed();
  }

  send(packet) {
    this._dbg.postMessage(
      JSON.stringify({
        type: "message",
        id: this._id,
        message: packet,
      })
    );
  }

  startBulkSend() {
    throw new Error("Can't send bulk data from worker threads!");
  }

  _onMessage(message) {
    const packet = JSON.parse(message);
    if (packet.type !== "message" || packet.id !== this._id || !this.hooks) {
      return;
    }

    this.hooks.onPacket(packet.message);
  }
}

exports.MainThreadWorkerDebuggerTransport = MainThreadWorkerDebuggerTransport;

/**
 * A transport that uses a WorkerDebuggerGlobalScope to send packets from a
 * worker thread to the main thread.
 */
function WorkerThreadWorkerDebuggerTransport(scope, id) {
  this._scope = scope;
  this._id = id;
  this._onMessage = this._onMessage.bind(this);
}

WorkerThreadWorkerDebuggerTransport.prototype = {
  constructor: WorkerThreadWorkerDebuggerTransport,

  ready: function() {
    this._scope.addEventListener("message", this._onMessage);
  },

  close: function() {
    this._scope.removeEventListener("message", this._onMessage);
    this.hooks?.onTransportClosed();
  },

  send: function(packet) {
    this._scope.postMessage(
      JSON.stringify({
        type: "message",
        id: this._id,
        message: packet,
      })
    );
  },

  startBulkSend: function() {
    throw new Error("Can't send bulk data from worker threads!");
  },

  _onMessage: function(event) {
    const packet = JSON.parse(event.data);
    if (packet.type !== "message" || packet.id !== this._id) {
      return;
    }

    if (this.hooks) {
      this.hooks.onPacket(packet.message);
    }
  },
};

exports.WorkerThreadWorkerDebuggerTransport = WorkerThreadWorkerDebuggerTransport;
