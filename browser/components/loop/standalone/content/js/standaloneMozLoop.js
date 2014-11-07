/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

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
   * @param jqXHR See jQuery docs
   * @param textStatus See jQuery docs
   * @param errorThrown See jQuery docs
   */
  function failureHandler(callback, jqXHR, textStatus, errorThrown) {
    var jsonErr = jqXHR && jqXHR.responseJSON || {};
    var message = "HTTP " + jqXHR.status + " " + errorThrown;

    // Create an error with server error `errno` code attached as a property
    var err = new Error(message);
    err.errno = jsonErr.errno;

    callback(err);
  }

  /**
   * StandaloneMozLoopRooms is used as part of StandaloneMozLoop to define
   * the rooms sub-object. We do it this way so that we can share the options
   * information from the parent.
   */
  var StandaloneMozLoopRooms = function(options) {
    options = options || {};
    if (!options.baseServerUrl) {
      throw new Error("missing required baseServerUrl");
    }

    this._baseServerUrl = options.baseServerUrl;
  };

  StandaloneMozLoopRooms.prototype = {
    /**
     * Internal function to actually perform a post to a room.
     *
     * @param {String} roomToken The rom token.
     * @param {String} sessionToken The sessionToken for the room if known
     * @param {Object} roomData The data to send with the request
     * @param {Array} expectedProps The expected properties we should receive from the
     *                              server
     * @param {Function} callback The callback for when the request completes. The
     *                            first parameter is non-null on error, the second parameter
     *                            is the response data.
     */
    _postToRoom: function(roomToken, sessionToken, roomData, expectedProps, callback) {
      var req = $.ajax({
        url:         this._baseServerUrl + "/rooms/" + roomToken,
        method:      "POST",
        contentType: "application/json",
        dataType:    "json",
        data: JSON.stringify(roomData),
        beforeSend: function(xhr) {
          if (sessionToken) {
            xhr.setRequestHeader("Authorization", "Basic " + btoa(sessionToken));
          }
        }
      });

      req.done(function(responseData) {
        try {
          callback(null, validate(responseData, expectedProps));
        } catch (err) {
          console.error("Error requesting call info", err.message);
          callback(err);
        }
      }.bind(this));

      req.fail(failureHandler.bind(this, callback));
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
      this._postToRoom(roomToken, null, {
        action: "join",
        displayName: mozL10n.get("rooms_display_name_guest"),
        clientMaxSize: ROOM_MAX_CLIENTS
      }, {
        apiKey: String,
        sessionId: String,
        sessionToken: String,
        expires: Number
      }, callback);
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
      }, callback);
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

      this._postToRoom(roomToken, sessionToken, {
        action: "leave",
        sessionToken: sessionToken
      }, null, callback);
    }
  };

  var StandaloneMozLoop = function(options) {
    options = options || {};
    if (!options.baseServerUrl) {
      throw new Error("missing required baseServerUrl");
    }

    this._baseServerUrl = options.baseServerUrl;

    this.rooms = new StandaloneMozLoopRooms(options);
  };

  return StandaloneMozLoop;
})(navigator.mozL10n);
