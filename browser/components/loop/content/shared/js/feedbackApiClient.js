/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.FeedbackAPIClient = (function($, _) {
  "use strict";

  /**
   * Feedback API client. Sends feedback data to an input.mozilla.com compatible
   * API.
   *
   * @param {String} baseUrl  Base API url (required)
   * @param {Object} defaults Defaults field values for that client.
   *
   * Required defaults:
   * - {String} product Product name (required)
   *
   * Optional defaults:
   * - {String} platform   Platform name, eg. "Windows 8", "Android", "Linux"
   * - {String} version    Product version, eg. "22b2", "1.1"
   * - {String} channel    Product channel, eg. "stable", "beta"
   * - {String} user_agent eg. Mozilla/5.0 (Mobile; rv:18.0) Gecko/18.0 Firefox/18.0
   *
   * @link  http://fjord.readthedocs.org/en/latest/api.html
   */
  function FeedbackAPIClient(baseUrl, defaults) {
    this.baseUrl = baseUrl;
    if (!this.baseUrl) {
      throw new Error("Missing required 'baseUrl' argument.");
    }

    this.defaults = defaults || {};
    // required defaults checks
    if (!this.defaults.hasOwnProperty("product")) {
      throw new Error("Missing required 'product' default.");
    }
  }

  FeedbackAPIClient.prototype = {
    /**
     * Supported field names by the feedback API.
     * @type {Array}
     */
    _supportedFields: ["happy",
                       "category",
                       "description",
                       "product",
                       "platform",
                       "version",
                       "channel",
                       "user_agent"],

    /**
     * Creates a formatted payload object compliant with the Feedback API spec
     * against validated field data.
     *
     * @param  {Object} fields Feedback initial values.
     * @return {Object}        Formatted payload object.
     * @throws {Error}         If provided values are invalid
     */
    _createPayload: function(fields) {
      if (typeof fields !== "object") {
        throw new Error("Invalid feedback data provided.");
      }

      Object.keys(fields).forEach(function(name) {
        if (this._supportedFields.indexOf(name) === -1) {
          throw new Error("Unsupported field " + name);
        }
      }, this);

      // Payload is basically defaults + fields merged in
      var payload = _.extend({}, this.defaults, fields);

      // Default description field value
      if (!fields.description) {
        payload.description = (fields.happy ? "Happy" : "Sad") + " User";
      }

      return payload;
    },

    /**
     * Sends feedback data.
     *
     * @param  {Object}   fields Feedback form data.
     * @param  {Function} cb     Callback(err, result)
     */
    send: function(fields, cb) {
      var req = $.ajax({
        url:         this.baseUrl,
        method:      "POST",
        contentType: "application/json",
        dataType:    "json",
        data: JSON.stringify(this._createPayload(fields))
      });

      req.done(function(result) {
        console.info("User feedback data have been submitted", result);
        cb(null, result);
      });

      req.fail(function(jqXHR, textStatus, errorThrown) {
        var message = "Error posting user feedback data";
        var httpError = jqXHR.status + " " + errorThrown;
        console.error(message, httpError, JSON.stringify(jqXHR.responseJSON));
        cb(new Error(message + ": " + httpError));
      });
    }
  };

  return FeedbackAPIClient;
})(jQuery, _);
