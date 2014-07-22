/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.StandaloneClient = (function($) {
  "use strict";

  // The expected properties to be returned from the POST /calls request.
  var expectedCallsProperties = [ "sessionId", "sessionToken", "apiKey" ];

  /**
   * Loop server standalone client.
   *
   * @param {Object} settings Settings object.
   */
  function StandaloneClient(settings) {
    settings = settings || {};
    if (!settings.baseServerUrl) {
      throw new Error("missing required baseServerUrl");
    }

    this.settings = settings;
  }

  StandaloneClient.prototype = {
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

      if (properties.length === 1) {
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
      var jsonErr = jqXHR && jqXHR.responseJSON || {};
      var message = "HTTP " + jqXHR.status + " " + errorThrown;

      // Logging the technical error to the console
      console.error("Server error", message, jsonErr);

      // Create an error with server error `errno` code attached as a property
      var err = new Error(message);
      err.errno = jsonErr.errno;

      cb(err);
    },

    /**
     * Posts a call request to the server for a call represented by the
     * loopToken. Will return the session data for the call.
     *
     * @param  {String} loopToken The loopToken representing the call
     * @param  {String} callType The type of media in the call, e.g.
     *                           "audio" or "audio-video"
     * @param  {Function} cb Callback(err, sessionData)
     */
    requestCallInfo: function(loopToken, callType, cb) {
      if (!loopToken) {
        throw new Error("missing required parameter loopToken");
      }

      var req = $.ajax({
        url:         this.settings.baseServerUrl + "/calls/" + loopToken,
        method:      "POST",
        contentType: "application/json",
        dataType:    "json",
        data: JSON.stringify({callType: callType})
      });

      req.done(function(sessionData) {
        try {
          cb(null, this._validate(sessionData, expectedCallsProperties));
        } catch (err) {
          console.log("Error requesting call info", err);
          cb(err);
        }
      }.bind(this));

      req.fail(this._failureHandler.bind(this, cb));
    },
  };

  return StandaloneClient;
})(jQuery);
