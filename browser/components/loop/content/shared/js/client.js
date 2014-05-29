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
     * @param testStatus See jQuery docs
     * @param errorThrown See jQuery docs
     */
    _failureHandler: function(cb, jqXHR, testStatus, errorThrown) {
      var error = "Unknown error.";
      if (jqXHR && jqXHR.responseJSON && jqXHR.responseJSON.error) {
        error = jqXHR.responseJSON.error;
      }
      var message = "HTTP error " + jqXHR.status + ": " +
        errorThrown + "; " + error;
      console.error(message);
      cb(new Error(message));
    },

    /**
     * Requests a call URL from the Loop server.
     *
     * @param  {String} simplepushUrl a registered Simple Push URL
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrl)
     */
    requestCallUrl: function(nickname, cb) {
      var endpoint = this.settings.baseServerUrl + "/call-url/",
          reqData  = {callerId: nickname};

      var req = $.post(endpoint, reqData, function(callUrlData) {
        try {
          cb(null, this._validate(callUrlData, ["call_url"]));
        } catch (err) {
          console.log("Error requesting call info", err);
          cb(err);
        }
      }.bind(this), "json");

      req.fail(this._failureHandler.bind(this, cb));
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
