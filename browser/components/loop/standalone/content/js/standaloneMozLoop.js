/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The StandaloneMozLoop implementation reflects that of the mozLoop API for Loop
 * in the desktop code. Not all functions are implemented.
 */
var loop = loop || {};
loop.StandaloneMozLoop = (function(mozL10n) {
  "use strict";

  /**
   * The maximum number of clients that we currently support.
   */
  var ROOM_MAX_CLIENTS = 2;
  var PUSH_SUBSCRIPTION = "pushSubscription";
  var BATCH_MESSAGE = "Batch";
  var MAX_LOOP_COUNT = 10;

  function cloneableError(err) {
    if (typeof err == "string") {
      err = new Error(err);
    }
    err.isError = true;
    return err;
  }

  /**
   * Validates a data object to confirm it has the specified properties.
   *
   * @param  {Object} data    The data object to verify
   * @param  {Array}  schema  The validation schema
   * @return Returns all properties if valid, or an empty object if no properties
   *         have been specified.
   */
  function validate(data, schema) {
    if (!schema) {
      return {};
    }

    return new loop.validate.Validator(schema).validate(data);
  }

  /**
   * Generic handler for XHR failures.
   *
   * @param {Function} callback Callback(err)
   * @param xhrReq
   */
  function failureHandler(callback, xhrReq) {
    var jsonErr = JSON.parse(xhrReq.responseText && xhrReq.responseText || "{}");
    var message = "HTTP " + xhrReq.status + " " + xhrReq.statusText;
    // Create an error with server error `errno` code attached as a property
    var err = new Error(message);
    err.errno = jsonErr.errno;

    callback(err);
  }

  /**
   * StandaloneLoopRooms is used to define the rooms sub-object. We do it this
   * way so that we can share the options information.
   */
  var StandaloneLoopRooms = {
    baseServerUrl: "",
    /**
     * Request information about a specific room from the server.
     *
     * @param {String}   roomToken Room identifier
     * @param {Function} callback  Function that will be invoked once the operation
     *                             finished. The first argument passed will be an
     *                             `Error` object or `null`. The second argument will
     *                             be the list of rooms, if it was fetched successfully.
     */
    get: function(roomToken, callback) {
      var url = this.baseServerUrl + "/rooms/" + roomToken;

      this._xhrReq = new XMLHttpRequest();
      this._xhrReq.open("GET", url, true);
      this._xhrReq.setRequestHeader("Content-type", "application/json");

      if (this.sessionToken) {
        this._xhrReq.setRequestHeader("Authorization", "Basic " + btoa(this.sessionToken));
      }

      this._xhrReq.onload = function() {
        var request = this._xhrReq;
        var responseJSON = JSON.parse(request.responseText || "{}");

        if (request.readyState === 4 && request.status >= 200 && request.status < 300) {
          try {
            // We currently only require things we need rather than everything possible.
            callback(null, validate(responseJSON, { roomUrl: String }));
          } catch (err) {
            console.error("Error requesting call info", err.message);
            callback(err);
          }
        } else {
          failureHandler(callback, request);
        }
      }.bind(this);

      this._xhrReq.send();
    },

    /**
     * Internal function to actually perform a post to a room.
     *
     * @param {String} roomToken    The room token.
     * @param {String} sessionToken The sessionToken for the room if known
     * @param {Object} roomData     The data to send with the request
     * @param {Array} expectedProps The expected properties we should receive from the
     *                              server
     * @param {Boolean} async       Set to true for an async request, false for sync.
     * @param {Function} callback The callback for when the request completes. The
     *                            first parameter is non-null on error, the second parameter
     *                            is the response data.
     */
    _postToRoom: function(roomToken, sessionToken, roomData, expectedProps,
                          async, callback) {
      var url = this.baseServerUrl + "/rooms/" + roomToken;
      var xhrReq = new XMLHttpRequest();

      xhrReq.open("POST", url, async);
      xhrReq.setRequestHeader("Content-type", "application/json");

      if (sessionToken) {
        xhrReq.setRequestHeader("Authorization", "Basic " + btoa(sessionToken));
      }

      xhrReq.onload = function() {
        var request = xhrReq;
        var responseJSON = JSON.parse(request.responseText || null);

        if (request.readyState === 4 && request.status >= 200 && request.status < 300) {
          try {
            callback(null, validate(responseJSON, expectedProps));
          } catch (err) {
            console.error("Error requesting call info", err.message);
            callback(err);
          }
        } else {
          failureHandler(callback, request);
        }
      }.bind(this, xhrReq);

      xhrReq.send(JSON.stringify(roomData));
    },

    /**
     * Joins a room
     *
     * @param {String} roomToken  The room token.
     * @param {Function} callback Function that will be invoked once the operation
     *                            finished. The first argument passed will be an
     *                            `Error` object or `null`.
     */
    join: function(roomToken, callback) {
      function callbackWrapper(err, result) {
        // XXX Save the sessionToken for purposes of get.
        // When bug 1103331 this can probably be removed.
        if (result) {
          this.sessionToken = result.sessionToken;
        }

        callback(err, result);
      }

      this._postToRoom(roomToken, null, {
        action: "join",
        displayName: mozL10n.get("rooms_display_name_guest"),
        clientMaxSize: ROOM_MAX_CLIENTS
      }, {
        apiKey: String,
        sessionId: String,
        sessionToken: String,
        expires: Number
      }, true, callbackWrapper.bind(this));
    },

    /**
     * Refreshes a room
     *
     * @param {String} roomToken    The room token.
     * @param {String} sessionToken The session token for the session that has been
     *                              joined
     * @param {Function} callback   Function that will be invoked once the operation
     *                              finished. The first argument passed will be an
     *                              `Error` object or `null`.
     */
    refreshMembership: function(roomToken, sessionToken, callback) {
      this._postToRoom(roomToken, sessionToken, {
        action: "refresh",
        sessionToken: sessionToken
      }, {
        expires: Number
      }, true, callback);
    },

    /**
     * Leaves a room. Although this is an sync function, no data is returned
     * from the server.
     *
     * @param {String} roomToken    The room token.
     * @param {String} sessionToken The session token for the session that has been
     *                              joined
     * @param {Function} callback   Optional. Function that will be invoked once the operation
     *                              finished. The first argument passed will be an
     *                              `Error` object or `null`.
     */
    leave: function(roomToken, sessionToken, callback) {
      if (!callback) {
        callback = function(error) {
          if (error) {
            console.error(error);
          }
        };
      }

      // We do this as a synchronous request in case this is closing the window.
      this._postToRoom(roomToken, sessionToken, {
        action: "leave",
        sessionToken: sessionToken
      }, null, false, callback);
    },

    /**
     * Forwards connection status to the server.
     *
     * @param {String} roomToken     The room token.
     * @param {String} sessionToken  The session token for the session that has been
     *                               joined.
     * @param {sharedActions.SdkStatus} status  The connection status.
     */
    sendConnectionStatus: function(roomToken, sessionToken, status) {
      this._postToRoom(roomToken, sessionToken, {
        action: "status",
        event: status.event,
        state: status.state,
        connections: status.connections,
        sendStreams: status.sendStreams,
        recvStreams: status.recvStreams
      }, null, true, function(error) {
        if (error) {
          console.error(error);
        }
      });
    }
  };

  var kMessageHandlers = {
    "Rooms:*": function(action, data, reply) {
      var funcName = action.split(":").pop();
      funcName = funcName.charAt(0).toLowerCase() + funcName.substr(1);

      if (funcName === PUSH_SUBSCRIPTION) {
        return;
      }

      if (typeof StandaloneLoopRooms[funcName] !== "function") {
        reply(cloneableError("Sorry, function '" + funcName + "' does not exist!"));
        return;
      }
      data.push(function(err, result) {
        reply(err ? cloneableError(err) : result);
      });
      StandaloneLoopRooms[funcName].apply(StandaloneLoopRooms, data);
    },

    /**
     * Stores a preference in the local storage for standalone.
     *
     * @param {Array}    data  Containing the the name of the pref and the value
     *                         to set.
     * @param {Function} reply Callback function to invoke when done.
     */
    SetLoopPref: function(data, reply) {
      var prefName = data[0];
      var value = data[1];
      localStorage.setItem(prefName, value);
      reply();
    },

    /**
     * Gets a preference from the local storage for standalone.
     *
     * @param {Array}    data  Containing the the name of the pref
     * @param {Function} reply Callback function to invoke when done with the
     *                         value of the pref.
     */
    GetLoopPref: function(data, reply) {
      var prefName = data[0];
      reply(localStorage.getItem(prefName));
    }
  };

  function handleBatchMessage(name, seq, data, reply) {
    var actions = data[0];
    if (!actions.length) {
      throw new Error("Ough, a batch call with no actions is not much " +
        "of a batch, now is it?");
    }

    // Since `handleBatchMessage` can be called recursively, but the replies are
    // collected and sent only once, we'll make sure only one exists for the
    // entire tail.
    // We count the amount of recursive calls, because we don't want any consumer
    // to cause an infinite loop, now do we?
    if (!("loopCount" in reply)) {
      reply.loopCount = 0;
    } else if (++reply.loopCount > MAX_LOOP_COUNT) {
      reply(cloneableError("Too many nested calls"));
      return;
    }

    var resultSet = {};
    var done = 0;
    actions.forEach(function(actionSet) {
      var actionSeq = actionSet[0];
      window.sendAsyncMessage(name, actionSet, function(result) {
        resultSet[actionSeq] = result;
        if (++done === actions.length) {
          reply(resultSet);
        }
      });
    });
  }

  var messageListeners = {};

  return function(options) {
    options = options || {};
    if (!options.baseServerUrl) {
      throw new Error("missing required baseServerUrl");
    }

    StandaloneLoopRooms.baseServerUrl = options.baseServerUrl;

    window.sendAsyncMessage = function(name, data, reply) {
      if (name !== "Loop:Message") {
        return;
      }

      var seq = data.shift();
      var action = data.shift();

      var actionParts = action.split(":");

      // The name that is supposed to match with a handler function is tucked inside
      // the second part of the message name. If all is well.
      var handlerName = actionParts.shift();

      if (!reply) {
        reply = function(result) {
          var listeners = messageListeners[name];
          if (!listeners || !listeners.length) {
            return;
          }
          var message = { data: [seq, result] };
          listeners.forEach(function(listener) {
            listener(message);
          });
        };
      }

      // First, check if this is a batch call.
      if (handlerName === BATCH_MESSAGE) {
        handleBatchMessage(name, seq, data, reply);
        return;
      }

      // Second, check if the message is meant for one of our Object APIs.
      // If so, a wildcard entry should exist for the message name in the
      // `kMessageHandlers` dictionary.
      var wildcardName = handlerName + ":*";
      if (kMessageHandlers[wildcardName]) {
        // Alright, pass the message forward.
        kMessageHandlers[wildcardName](action, data, reply);
        // Aaaaand we're done.
        return;
      }

      if (!kMessageHandlers[handlerName]) {
        var msg = "Ouch, no message handler available for '" + handlerName + "'";
        console.error(msg);
        reply(cloneableError(msg));
        return;
      }

      kMessageHandlers[handlerName](data, reply);
    };

    window.addMessageListener = function(name, func) {
      if (!messageListeners[name]) {
        messageListeners[name] = [];
      }

      if (messageListeners[name].indexOf(func) === -1) {
        messageListeners[name].push(func);
      }
    };

    window.removeMessageListener = function(name, func) {
      if (!messageListeners[name]) {
        return;
      }

      var idx = messageListeners[name].indexOf(func);
      if (idx !== -1) {
        messageListeners[name].splice(idx, 1);
      }
      if (!messageListeners[name].length) {
        delete messageListeners[name];
      }
    };
  };
})(navigator.mozL10n);
