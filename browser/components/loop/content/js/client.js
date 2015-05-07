/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint esnext:true */
/* global loop:true, hawk, deriveHawkCredentials */

var loop = loop || {};
loop.Client = (function($) {
  "use strict";

  // THe expected properties to be returned from the POST /calls request.
  var expectedPostCallProperties = [
    "apiKey", "callId", "progressURL",
    "sessionId", "sessionToken", "websocketToken"
  ];

  /**
   * Loop server client.
   *
   * @param {Object} settings Settings object.
   */
  function Client(settings) {
    if (!settings) {
      settings = {};
    }
    // allowing an |in| test rather than a more type || allows us to dependency
    // inject a non-existent mozLoop
    if ("mozLoop" in settings) {
      this.mozLoop = settings.mozLoop;
    } else {
      this.mozLoop = navigator.mozLoop;
    }

    this.settings = settings;
  }

  Client.prototype = {
    /**
     * Validates a data object to confirm it has the specified properties.
     *
     * @param  {Object} The data object to verify
     * @param  {Array} The list of properties to verify within the object
     * @return This returns either the specific property if only one
     *         property is specified, or it returns all properties
     */
    _validate: function(data, properties) {
      if (typeof data !== "object") {
        throw new Error("Invalid data received from server");
      }

      properties.forEach(function (property) {
        if (!data.hasOwnProperty(property)) {
          throw new Error("Invalid data received from server - missing " +
            property);
        }
      });

      if (properties.length == 1) {
        return data[properties[0]];
      }

      return data;
    },

    /**
     * Generic handler for XHR failures.
     *
     * @param {Function} cb Callback(err)
     * @param {Object} error See MozLoopAPI.hawkRequest
     */
    _failureHandler: function(cb, error) {
      var message = "HTTP " + error.code + " " + error.error + "; " + error.message;
      console.error(message);
      cb(error);
    },

    /**
     * Sets up an outgoing call, getting the relevant data from the server.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     * - result an object of the obtained data for starting the call, if successful
     *
     * @param {Array} calleeIds an array of emails and phone numbers.
     * @param {String} callType the type of call.
     * @param {Function} cb Callback(err, result)
     */
    setupOutgoingCall: function(calleeIds, callType, cb) {
      // For direct calls, we only ever use the logged-in session. Direct
      // calls by guests aren't valid.
      this.mozLoop.hawkRequest(this.mozLoop.LOOP_SESSION_TYPE.FXA,
        "/calls", "POST", {
          calleeId: calleeIds,
          callType: callType,
          channel: this.mozLoop.appVersionInfo ?
                   this.mozLoop.appVersionInfo.channel : "unknown"
        },
        function (err, responseText) {
          if (err) {
            this._failureHandler(cb, err);
            return;
          }

          try {
            var postData = JSON.parse(responseText);

            var outgoingCallData = this._validate(postData,
              expectedPostCallProperties);

            cb(null, outgoingCallData);
          } catch (err) {
            console.log("Error requesting call info", err);
            cb(err);
          }
        }.bind(this)
      );
    }
  };

  return Client;
})(jQuery);
