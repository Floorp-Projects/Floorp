/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Objects and functions were cherry-picked from devtools/shared/client/main.js, in
// order to make the console launchpad Worker.

// mock, we only need DebuggerClient.requester
const DebuggerClient = function(transport) {};

/**
 * A declarative helper for defining methods that send requests to the server.
 *
 * @param packetSkeleton
 *        The form of the packet to send. Can specify fields to be filled from
 *        the parameters by using the |arg| function.
 * @param before
 *        The function to call before sending the packet. Is passed the packet,
 *        and the return value is used as the new packet. The |this| context is
 *        the instance of the client object we are defining a method for.
 * @param after
 *        The function to call after the response is received. It is passed the
 *        response, and the return value is considered the new response that
 *        will be passed to the callback. The |this| context is the instance of
 *        the client object we are defining a method for.
 * @return Request
 *         The `Request` object that is a Promise object and resolves once
 *         we receive the response. (See request method for more details)
 */
DebuggerClient.requester = function(packetSkeleton, config = {}) {
  const { before, after } = config;
  return function(...args) {
    let outgoingPacket = {
      to: packetSkeleton.to || this.actor
    };

    let maxPosition = -1;
    for (const k of Object.keys(packetSkeleton)) {
      if (packetSkeleton[k] instanceof DebuggerClient.Argument) {
        const { position } = packetSkeleton[k];
        outgoingPacket[k] = packetSkeleton[k].getArgument(args);
        maxPosition = Math.max(position, maxPosition);
      } else {
        outgoingPacket[k] = packetSkeleton[k];
      }
    }

    if (before) {
      outgoingPacket = before.call(this, outgoingPacket);
    }

    return this.request(outgoingPacket, (response) => {
      if (after) {
        const { from } = response;
        response = after.call(this, response);
        if (!response.from) {
          response.from = from;
        }
      }

      // The callback is always the last parameter.
      const thisCallback = args[maxPosition + 1];
      if (thisCallback) {
        thisCallback(response);
      }
      return response;
    }, "DebuggerClient.requester request callback");
  };
};

function arg(pos) {
  return new DebuggerClient.Argument(pos);
}

DebuggerClient.Argument = function(position) {
  this.position = position;
};

DebuggerClient.Argument.prototype.getArgument = function(params) {
  if (!(this.position in params)) {
    throw new Error("Bad index into params: " + this.position);
  }
  return params[this.position];
};

module.exports = { arg, DebuggerClient };
