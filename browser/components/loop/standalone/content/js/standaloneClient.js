/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.StandaloneClient = (function() {
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
     * @param  {Object} data        The data object to verify
     * @param  {Array}  properties  The list of properties to verify within the object
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
     * @param xhrReq
     */
    _failureHandler: function(cb, xhrReq) {
      var jsonErr = JSON.parse(xhrReq.responseText && xhrReq.responseText || "{}");
      var message = "HTTP " + xhrReq.status + " " + xhrReq.statusText;

      // Logging the technical error to the console
      console.error("Server error", message, jsonErr);

      // Create an error with server error `errno` code attached as a property
      var err = new Error(message);
      err.errno = jsonErr.errno;

      cb(err);
    },

    /**
     * Makes a request for url creation date for standalone UI
     *
     * @param {String} loopToken The loopToken representing the call
     * @param {Function} cb Callback(err, callUrlInfo)
     *
     **/
    requestCallUrlInfo: function(loopToken, cb) {
      if (!loopToken) {
        throw new Error("Missing required parameter loopToken");
      }
      if (!cb) {
        throw new Error("Missing required callback function");
      }

      var url = this.settings.baseServerUrl + "/calls/" + loopToken;
      var xhrReq = new XMLHttpRequest();

      xhrReq.open("GET", url, true);
      xhrReq.setRequestHeader("Content-type", "application/json");

      xhrReq.onload = function() {
        var request = xhrReq;
        var responseJSON = JSON.parse(request.responseText || null);

        if (request.readyState === 4 && request.status >= 200 && request.status < 300) {
          try {
            cb(null, responseJSON);
          } catch (err) {
            console.error("Error requesting call info", err.message);
            cb(err);
          }
        } else {
          this._failureHandler(cb, request);
        }
      }.bind(this, xhrReq);

      xhrReq.send();
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

      var url = this.settings.baseServerUrl + "/calls/" + loopToken;
      var xhrReq = new XMLHttpRequest();

      xhrReq.open("POST", url, true);
      xhrReq.setRequestHeader("Content-type", "application/json");

      xhrReq.onload = function() {
        var request = xhrReq;
        var responseJSON = JSON.parse(request.responseText || null);

        if (request.readyState === 4 && request.status >= 200 && request.status < 300) {
          try {
            cb(null, this._validate(responseJSON, expectedCallsProperties));
          } catch (err) {
            console.error("Error requesting call info", err.message);
            cb(err);
          }
        } else {
          this._failureHandler(cb, request);
        }
      }.bind(this, xhrReq);

      xhrReq.send(JSON.stringify({callType: callType, channel: "standalone"}));
    }
  };

  return StandaloneClient;
})();
