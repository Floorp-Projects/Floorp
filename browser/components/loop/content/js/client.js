/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint esnext:true */
/* global loop:true, hawk, deriveHawkCredentials */

var loop = loop || {};
loop.Client = (function($) {
  "use strict";

  // The expected properties to be returned from the POST /call-url/ request.
  var expectedCallUrlProperties = ["callUrl", "expiresAt"];

  // The expected properties to be returned from the GET /calls request.
  var expectedCallProperties = ["calls"];

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
     * Ensures the client is registered with the push server.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     *
     * @param {Function} cb Callback(err)
     */
    _ensureRegistered: function(cb) {
      this.mozLoop.ensureRegistered(function(error) {
        if (error) {
          console.log("Error registering with Loop server, code: " + error);
          cb(error);
          return;
        } else {
          cb(null);
        }
      });
    },

    /**
     * Internal handler for requesting a call url from the server.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     * - callUrlData an object of the obtained call url data if successful:
     * -- callUrl: The url of the call
     * -- expiresAt: The amount of hours until expiry of the url
     *
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrlData)
     */
    _requestCallUrlInternal: function(nickname, cb) {
      var sessionType;
      if (this.mozLoop.userProfile) {
        sessionType = this.mozLoop.LOOP_SESSION_TYPE.FXA;
      } else {
        sessionType = this.mozLoop.LOOP_SESSION_TYPE.GUEST;
      }

      this.mozLoop.hawkRequest(sessionType, "/call-url/", "POST",
                               {callerId: nickname},
        function (error, responseText) {
          if (error) {
            this._telemetryAdd("LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS", false);
            this._failureHandler(cb, error);
            return;
          }

          try {
            var urlData = JSON.parse(responseText);

            // This throws if the data is invalid, in which case only the failure
            // telemetry will be recorded.
            var returnData = this._validate(urlData, expectedCallUrlProperties);

            this._telemetryAdd("LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS", true);
            cb(null, returnData);
          } catch (err) {
            this._telemetryAdd("LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS", false);
            console.log("Error requesting call info", err);
            cb(err);
          }
        }.bind(this));
    },

    /**
     * Block call URL based on the token identifier
     *
     * @param {string} token Conversation identifier used to block the URL
     * @param {mozLoop.LOOP_SESSION_TYPE} sessionType The type of session which
     *                                                the url belongs to.
     * @param {function} cb Callback function used for handling an error
     *                      response. XXX The incoming call panel does not
     *                      exist after the block button is clicked therefore
     *                      it does not make sense to display an error.
     **/
    deleteCallUrl: function(token, sessionType, cb) {
      this._ensureRegistered(function(err) {
        if (err) {
          cb(err);
          return;
        }

        this._deleteCallUrlInternal(token, sessionType, cb);
      }.bind(this));
    },

    _deleteCallUrlInternal: function(token, sessionType, cb) {
      function deleteRequestCallback(error, responseText) {
        if (error) {
          this._failureHandler(cb, error);
          return;
        }

        try {
          cb(null);
        } catch (err) {
          console.log("Error deleting call info", err);
          cb(err);
        }
      }

      this.mozLoop.hawkRequest(sessionType,
                               "/call-url/" + token, "DELETE", null,
                               deleteRequestCallback.bind(this));
    },

    /**
     * Requests a call URL from the Loop server. It will note the
     * expiry time for the url with the mozLoop api.  It will select the
     * appropriate hawk session to use based on whether or not the user
     * is currently logged into a Firefox account profile.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     * - callUrlData an object of the obtained call url data if successful:
     * -- callUrl: The url of the call
     * -- expiresAt: The amount of hours until expiry of the url
     *
     * @param  {String} simplepushUrl a registered Simple Push URL
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrlData)
     */
    requestCallUrl: function(nickname, cb) {
      this._ensureRegistered(function(err) {
        if (err) {
          cb(err);
          return;
        }

        this._requestCallUrlInternal(nickname, cb);
      }.bind(this));
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
    },

    /**
     * Adds a value to a telemetry histogram, ignoring errors.
     *
     * @param  {string}  histogramId Name of the telemetry histogram to update.
     * @param  {integer} value       Value to add to the histogram.
     */
    _telemetryAdd: function(histogramId, value) {
      try {
        this.mozLoop.telemetryAdd(histogramId, value);
      } catch (err) {
        console.error("Error recording telemetry", err);
      }
    },
  };

  return Client;
})(jQuery);
