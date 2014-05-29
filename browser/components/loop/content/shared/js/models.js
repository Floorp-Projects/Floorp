/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.models = (function() {
  "use strict";

  /**
   * Conversation model.
   */
  var ConversationModel = Backbone.Model.extend({
    defaults: {
      callerId:     undefined, // Loop caller id
      loopToken:    undefined, // Loop conversation token
      sessionId:    undefined, // TB session id
      sessionToken: undefined, // TB session token
      apiKey:       undefined  // TB api key
    },

    /**
     * Initiates a conversation, requesting call session information to the Loop
     * server and updates appropriately the current model attributes with the
     * data.
     *
     * Triggered events:
     *
     * - `session:ready` when the session information have been successfully
     *   retrieved from the server;
     * - `session:error` when the request failed.
     *
     * @param  {String} baseServerUrl The server URL
     * @throws {Error}  If no baseServerUrl is given
     * @throws {Error}  If no conversation token is set
     */
    initiate: function(baseServerUrl) {
      if (!baseServerUrl) {
        throw new Error("baseServerUrl arg must be passed to initiate()");
      }

      if (!this.get("loopToken")) {
        throw new Error("missing required attribute loopToken");
      }

      // Check if the session is already set
      if (this.isSessionReady()) {
        return this.trigger("session:ready", this);
      }

      var request = $.ajax({
        url:         baseServerUrl + "/calls/" + this.get("loopToken"),
        method:      "POST",
        contentType: "application/json",
        data:        JSON.stringify({}),
        dataType:    "json"
      });

      request.done(this.setReady.bind(this));

      request.fail(function(xhr, _, statusText) {
        var serverError = xhr.status + " " + statusText;
        if (typeof xhr.responseJSON === "object" && xhr.responseJSON.error) {
          serverError += "; " + xhr.responseJSON.error;
        }
        this.trigger("session:error", new Error(
          "Retrieval of session information failed: HTTP " + serverError));
      }.bind(this));
    },

    /**
     * Checks that the session is ready.
     *
     * @return {Boolean}
     */
    isSessionReady: function() {
      return !!this.get("sessionId");
    },

    /**
     * Sets session information and triggers the `session:ready` event.
     *
     * @param {Object} sessionData Conversation session information.
     */
    setReady: function(sessionData) {
      // Explicit property assignment to prevent later "surprises"
      this.set({
        sessionId:    sessionData.sessionId,
        sessionToken: sessionData.sessionToken,
        apiKey:       sessionData.apiKey
      }).trigger("session:ready", this);
      return this;
    }
  });

  /**
   * Notification model.
   */
  var NotificationModel = Backbone.Model.extend({
    defaults: {
      level: "info",
      message: ""
    }
  });

  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({
    model: NotificationModel
  });

  return {
    ConversationModel: ConversationModel,
    NotificationCollection: NotificationCollection,
    NotificationModel: NotificationModel
  };
})();
