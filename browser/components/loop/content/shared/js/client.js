/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.Client = (function($) {
  "use strict";

  /**
   * Loop server client.
   *
   * @param {Object} settings Settings object.
   */
  function Client(settings) {
    settings = settings || {};
    if (!settings.hasOwnProperty("baseServerUrl") ||
        !settings.baseServerUrl) {
      throw new Error("missing required baseServerUrl");
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

      if (properties.length <= 1) {
        return data[properties[0]];
      }

      return data;
    },

    /**
     * Generic handler for XHR failures.
     *
     * @param {Function} cb Callback(err)
     * @param jqXHR See jQuery docs
     * @param textStatus See jQuery docs
     * @param errorThrown See jQuery docs
     */
    _failureHandler: function(cb, jqXHR, textStatus, errorThrown) {
      var error = "Unknown error.",
          jsonRes = jqXHR && jqXHR.responseJSON || {};
      // Received error response format:
      // { "status": "errors",
      //   "errors": [{
      //      "location": "url",
      //      "name": "token",
      //      "description": "invalid token"
      // }]}
      if (jsonRes.status === "errors" && Array.isArray(jsonRes.errors)) {
        error = "Details: " + jsonRes.errors.map(function(err) {
          return Object.keys(err).map(function(field) {
            return field + ": " + err[field];
          }).join(", ");
        }).join("; ");
      }
      var message = "HTTP " + jqXHR.status + " " + errorThrown +
          "; " + error;
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
      navigator.mozLoop.ensureRegistered(function(err) {
        cb(err);
      }.bind(this));
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
      var endpoint = this.settings.baseServerUrl + "/call-url/",
          reqData  = {callerId: nickname};

      var req = $.post(endpoint, reqData, function(callUrlData) {
        try {
          cb(null, this._validate(callUrlData, ["call_url", "expiresAt"]));

          var expiresHours = this._hoursToSeconds(callUrlData.expiresAt);
          navigator.mozLoop.noteCallUrlExpiry(expiresHours);
        } catch (err) {
          console.log("Error requesting call info", err);
          cb(err);
        }
      }.bind(this), "json");

      req.fail(this._failureHandler.bind(this, cb));
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
      if (!version) {
        throw new Error("missing required parameter version");
      }

      var endpoint = this.settings.baseServerUrl + "/calls";

      // XXX It is likely that we'll want to move some of this to whatever
      // opens the chat window, but we'll need to decide that once we make a
      // decision on chrome versus content, and know if we're going with
      // LoopService or a frameworker.
      var req = $.get(endpoint + "?version=" + version, function(callsData) {
        try {
          cb(null, this._validate(callsData, ["calls"]));
        } catch (err) {
          console.log("Error requesting calls info", err);
          cb(err);
        }
      }.bind(this), "json");

      req.fail(this._failureHandler.bind(this, cb));
    },

    /**
     * Posts a call request to the server for a call represented by the
     * loopToken. Will return the session data for the call.
     *
     * @param  {String} loopToken The loopToken representing the call
     * @param  {Function} cb Callback(err, sessionData)
     */
    requestCallInfo: function(loopToken, cb) {
      if (!loopToken) {
        throw new Error("missing required parameter loopToken");
      }

      var req = $.ajax({
        url:         this.settings.baseServerUrl + "/calls/" + loopToken,
        method:      "POST",
        contentType: "application/json",
        data:        JSON.stringify({}),
        dataType:    "json"
      });

      req.done(function(sessionData) {
        try {
          cb(null, this._validate(sessionData, [
            "sessionId", "sessionToken", "apiKey"
          ]));
        } catch (err) {
          console.log("Error requesting call info", err);
          cb(err);
        }
      }.bind(this));

      req.fail(this._failureHandler.bind(this, cb));
    }
  };

  return Client;
})(jQuery);
