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
     * @param  {Function} cb Callback(err, callUrl)
     */
    requestCallUrl: function(simplepushUrl, cb) {
      var endpoint = this.settings.baseApiUrl + "/call-url/",
          reqData = {simplepushUrl: simplepushUrl};

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

      req.fail(function(xhr, type, err) {
        var error = "Unknown error.";
        if (xhr && xhr.responseJSON && xhr.responseJSON.error) {
          error = xhr.responseJSON.error;
        }
        var message = "HTTP error " + xhr.status + ": " + err + "; " + error;
        console.error(message);
        cb(new Error(message));
      });
    }
  };

  return Client;
})(jQuery);
