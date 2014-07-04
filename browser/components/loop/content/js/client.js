/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, hawk, deriveHawkCredentials */

var loop = loop || {};
loop.Client = (function($) {
  "use strict";

  // The expected properties to be returned from the POST /call-url/ request.
  const expectedCallUrlProperties = ["callUrl", "expiresAt"];

  // The expected properties to be returned from the GET /calls request.
  const expectedCallProperties = ["calls"];

  /**
   * Loop server client.
   *
   * @param {Object} settings Settings object.
   */
  function Client(settings = {}) {

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
     * Converts from hours to seconds
     */
    _hoursToSeconds: function(value) {
      return value * 60 * 60;
    },

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
      cb(new Error(message));
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
      this.mozLoop.ensureRegistered(cb);
    },

    /**
     * Internal handler for requesting a call url from the server.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     * - callUrlData an object of the obtained call url data if successful:
     * -- call_url: The url of the call
     * -- expiresAt: The amount of hours until expiry of the url
     *
     * @param  {String} simplepushUrl a registered Simple Push URL
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrlData)
     */
    _requestCallUrlInternal: function(nickname, cb) {
      this.mozLoop.hawkRequest("/call-url/", "POST", {callerId: nickname},
                               (error, responseText) => {
        if (error) {
          this._failureHandler(cb, error);
          return;
        }

        try {
          var urlData = JSON.parse(responseText);

          // XXX Support an alternate call_url property for
          // backwards compatibility whilst we switch over servers.
          // Bug 1033988 will want to remove these two lines.
          if (urlData.call_url)
            urlData.callUrl = urlData.call_url;

          cb(null, this._validate(urlData, expectedCallUrlProperties));

          var expiresHours = this._hoursToSeconds(urlData.expiresAt);
          this.mozLoop.noteCallUrlExpiry(expiresHours);
        } catch (err) {
          console.log("Error requesting call info", err);
          cb(err);
        }
      });
    },

    /**
     * Requests a call URL from the Loop server. It will note the
     * expiry time for the url with the mozLoop api.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     * - callUrlData an object of the obtained call url data if successful:
     * -- call_url: The url of the call
     * -- expiresAt: The amount of hours until expiry of the url
     *
     * @param  {String} simplepushUrl a registered Simple Push URL
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrlData)
     */
    requestCallUrl: function(nickname, cb) {
      this._ensureRegistered(function(err) {
        if (err) {
          console.log("Error registering with Loop server, code: " + err);
          cb(err);
          return;
        }

        this._requestCallUrlInternal(nickname, cb);
      }.bind(this));
    },

    /**
     * Requests call information from the server for all calls since the
     * given version.
     *
     * @param  {String} version the version identifier from the push
     *                          notification
     * @param  {Function} cb Callback(err, calls)
     */
    requestCallsInfo: function(version, cb) {
      // XXX It is likely that we'll want to move some of this to whatever
      // opens the chat window, but we'll need to decide on this in bug 1002418
      if (!version) {
        throw new Error("missing required parameter version");
      }

      this.mozLoop.hawkRequest("/calls?version=" + version, "GET", null,
                               (error, responseText) => {
        if (error) {
          this._failureHandler(cb, error);
          return;
        }

        try {
          var callsData = JSON.parse(responseText);

          cb(null, this._validate(callsData, expectedCallProperties));
        } catch (err) {
          console.log("Error requesting calls info", err);
          cb(err);
        }
      });
    },
  };

  return Client;
})(jQuery);
