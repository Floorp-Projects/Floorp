/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global uneval */

const { CC } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { dumpn } = DevToolsUtils;
const flags = require("devtools/shared/flags");
const StreamUtils = require("devtools/shared/transport/stream-utils");
const promise = require("promise");
const defer = require("devtools/shared/defer");

loader.lazyGetter(this, "Pipe", () => {
  return CC("@mozilla.org/pipe;1", "nsIPipe", "init");
});

/**
 * An adapter that handles data transfers between the debugger client and
 * server when they both run in the same process. It presents the same API as
 * DebuggerTransport, but instead of transmitting serialized messages across a
 * connection it merely calls the packet dispatcher of the other side.
 *
 * @param other LocalDebuggerTransport
 *        The other endpoint for this debugger connection.
 *
 * @see DebuggerTransport
 */
function LocalDebuggerTransport(other) {
  this.other = other;
  this.hooks = null;

  // A packet number, shared between this and this.other. This isn't used by the
  // protocol at all, but it makes the packet traces a lot easier to follow.
  this._serial = this.other ? this.other._serial : { count: 0 };
  this.close = this.close.bind(this);
}

LocalDebuggerTransport.prototype = {
  /**
   * Transmit a message by directly calling the onPacket handler of the other
   * endpoint.
   */
  send: function(packet) {
    const serial = this._serial.count++;
    if (flags.wantLogging) {
      // Check 'from' first, as 'echo' packets have both.
      if (packet.from) {
        dumpn("Packet " + serial + " sent from " + uneval(packet.from));
      } else if (packet.to) {
        dumpn("Packet " + serial + " sent to " + uneval(packet.to));
      }
    }
    this._deepFreeze(packet);
    const other = this.other;
    if (other) {
      DevToolsUtils.executeSoon(DevToolsUtils.makeInfallible(() => {
        // Avoid the cost of JSON.stringify() when logging is disabled.
        if (flags.wantLogging) {
          dumpn("Received packet " + serial + ": " + JSON.stringify(packet, null, 2));
        }
        if (other.hooks) {
          other.hooks.onPacket(packet);
        }
      }, "LocalDebuggerTransport instance's this.other.hooks.onPacket"));
    }
  },

  /**
   * Send a streaming bulk packet directly to the onBulkPacket handler of the
   * other endpoint.
   *
   * This case is much simpler than the full DebuggerTransport, since there is
   * no primary stream we have to worry about managing while we hand it off to
   * others temporarily.  Instead, we can just make a single use pipe and be
   * done with it.
   */
  startBulkSend: function({actor, type, length}) {
    const serial = this._serial.count++;

    dumpn("Sent bulk packet " + serial + " for actor " + actor);
    if (!this.other) {
      const error = new Error("startBulkSend: other side of transport missing");
      return promise.reject(error);
    }

    const pipe = new Pipe(true, true, 0, 0, null);

    DevToolsUtils.executeSoon(DevToolsUtils.makeInfallible(() => {
      dumpn("Received bulk packet " + serial);
      if (!this.other.hooks) {
        return;
      }

      // Receiver
      const deferred = defer();
      const packet = {
        actor: actor,
        type: type,
        length: length,
        copyTo: (output) => {
          const copying =
          StreamUtils.copyStream(pipe.inputStream, output, length);
          deferred.resolve(copying);
          return copying;
        },
        stream: pipe.inputStream,
        done: deferred
      };

      this.other.hooks.onBulkPacket(packet);

      // Await the result of reading from the stream
      deferred.promise.then(() => pipe.inputStream.close(), this.close);
    }, "LocalDebuggerTransport instance's this.other.hooks.onBulkPacket"));

    // Sender
    const sendDeferred = defer();

    // The remote transport is not capable of resolving immediately here, so we
    // shouldn't be able to either.
    DevToolsUtils.executeSoon(() => {
      const copyDeferred = defer();

      sendDeferred.resolve({
        copyFrom: (input) => {
          const copying =
          StreamUtils.copyStream(input, pipe.outputStream, length);
          copyDeferred.resolve(copying);
          return copying;
        },
        stream: pipe.outputStream,
        done: copyDeferred
      });

      // Await the result of writing to the stream
      copyDeferred.promise.then(() => pipe.outputStream.close(), this.close);
    });

    return sendDeferred.promise;
  },

  /**
   * Close the transport.
   */
  close: function() {
    if (this.other) {
      // Remove the reference to the other endpoint before calling close(), to
      // avoid infinite recursion.
      const other = this.other;
      this.other = null;
      other.close();
    }
    if (this.hooks) {
      try {
        this.hooks.onClosed();
      } catch (ex) {
        console.error(ex);
      }
      this.hooks = null;
    }
  },

  /**
   * An empty method for emulating the DebuggerTransport API.
   */
  ready: function() {},

  /**
   * Helper function that makes an object fully immutable.
   */
  _deepFreeze: function(object) {
    Object.freeze(object);
    for (const prop in object) {
      // Freeze the properties that are objects, not on the prototype, and not
      // already frozen. Note that this might leave an unfrozen reference
      // somewhere in the object if there is an already frozen object containing
      // an unfrozen object.
      if (object.hasOwnProperty(prop) && typeof object === "object" &&
          !Object.isFrozen(object)) {
        this._deepFreeze(object[prop]);
      }
    }
  }
};

exports.LocalDebuggerTransport = LocalDebuggerTransport;
