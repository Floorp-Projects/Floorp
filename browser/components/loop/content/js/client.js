/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.Client = (function($) {
  "use strict";

  /**
   * Loop server client.
   *
   * @param {Object} settings Settings object.
   */
  function Client(settings) {
    settings = settings || {};
    if (!settings.hasOwnProperty("baseApiUrl")) {
      throw new Error("missing required baseApiUrl");
    }
    this.settings = settings;
  }

  Client.prototype = {
    /**
     * Requests a call URL to the Loop server.
     *
     * @param  {String} simplepushUrl a registered Simple Push URL
     * @param  {string} nickname the nickname of the future caller
     * @param  {Function} cb Callback(err, callUrl)
     */
    requestCallUrl: function(nickname, cb) {
      var endpoint = this.settings.baseApiUrl + "/call-url/",
          reqData  = {callerId: nickname};

      function validate(callUrlData) {
        if (typeof callUrlData !== "object" ||
            !callUrlData.hasOwnProperty("call_url")) {
          var message = "Invalid call url data received";
          console.error(message, callUrlData);
          throw new Error(message);
        }
        return callUrlData.call_url;
      }

      var req = $.post(endpoint, reqData, function(callUrlData) {
        try {
          cb(null, validate(callUrlData));
        } catch (err) {
          cb(err);
        }
      }, "json");

      req.fail(function(jqXHR, testStatus, errorThrown) {
        var error = "Unknown error.";
        if (jqXHR && jqXHR.responseJSON && jqXHR.responseJSON.error) {
          error = jqXHR.responseJSON.error;
        }
        var message = "HTTP error " + jqXHR.status + ": " +
          errorThrown + "; " + error;
        console.error(message);
        cb(new Error(message));
      });
    },

    /**
     * Requests call information from the server
     *
     * @param  {Function} cb Callback(err, calls)
     */
    requestCallsInfo: function(cb) {
      var endpoint = this.settings.baseApiUrl + "/calls";

      // We do a basic validation here that we have an object,
      // and pass the full information back.
      function validate(callsData) {
        if (typeof callsData !== "object" ||
            !callsData.hasOwnProperty("calls")) {
          var message = "Invalid calls data received";
          console.error(message, callsData);
          throw new Error(message);
        }
        return callsData.calls;
      }

      // XXX We'll want to figure out a way to store the version from each
      // request here. As this is typically the date, we just need to store the
      // time last requested.
      // XXX It is likely that we'll want to move some of this to whatever
      // opens the chat window.
      var req = $.get(endpoint + "?version=0", function(callsData) {
        try {
          cb(null, validate(callsData));
        } catch (err) {
          cb(err);
        }
      }, "json");

      req.fail(function(jqXHR, testStatus, errorThrown) {
        var error = "Unknown error.";
        if (jqXHR && jqXHR.responseJSON && jqXHR.responseJSON.error) {
          error = jqXHR.responseJSON.error;
        }
        var message = "HTTP error " + jqXHR.status + ": " +
          errorThrown + "; " + error;
        console.error(message);
        cb(new Error(message));
      });
    }
  };

  return Client;
})(jQuery);
