/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.actions = (function() {
  "use strict";

  /**
   * Actions are events that are triggered by the user, e.g. clicking a button,
   * or by an async event, e.g. status received.
   *
   * They should be dispatched to stores via the dispatcher.
   */

  function Action(name, schema, values) {
    var validatedData = new loop.validate.Validator(schema || {})
                                         .validate(values || {});
    for (var prop in validatedData)
      this[prop] = validatedData[prop];

    this.name = name;
  }

  Action.define = function(name, schema) {
    return Action.bind(null, name, schema);
  };

  return {
    /**
     * Used to trigger gathering of initial call data.
     */
    GatherCallData: Action.define("gatherCallData", {
      // XXX This may change when bug 1072323 is implemented.
      // Optional: Specify the calleeId for an outgoing call
      calleeId: [String, null],
      // Specify the callId for an incoming call.
      callId: [String, null]
    }),

    /**
     * Used to cancel call setup.
     */
    CancelCall: Action.define("cancelCall", {
    }),

    /**
     * Used to initiate connecting of a call with the relevant
     * sessionData.
     */
    ConnectCall: Action.define("connectCall", {
      // This object contains the necessary details for the
      // connection of the websocket, and the SDK
      sessionData: Object
    }),

    /**
     * Used for notifying of connection progress state changes.
     * The connection refers to the overall connection flow as indicated
     * on the websocket.
     */
    ConnectionProgress: Action.define("connectionProgress", {
      // The new connection state
      state: String
    }),

    /**
     * Used for notifying of connection failures.
     */
    ConnectionFailure: Action.define("connectionFailure", {
      // A string relating to the reason the connection failed.
      reason: String
    })
  };
})();
