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
      loopVersion:  undefined, // Loop version for /calls/ information. This
                               // is the version received from the push
                               // notification and is used by the server to
                               // determine the pending calls
      sessionId:    undefined, // OT session id
      sessionToken: undefined, // OT session token
      apiKey:       undefined  // OT api key
    },

    /**
     * Initiates a conversation, requesting call session information to the Loop
     * server and updates appropriately the current model attributes with the
     * data.
     *
     * Available options:
     *
     * - {String} baseServerUrl The server URL
     * - {Boolean} outgoing Set to true if this model represents the
     *                            outgoing call.
     *
     * Triggered events:
     *
     * - `session:ready` when the session information have been successfully
     *   retrieved from the server;
     * - `session:error` when the request failed.
     *
     * @param {Object} options Options object
     */
    initiate: function(options) {
      // Check if the session is already set
      if (this.isSessionReady()) {
        return this.trigger("session:ready", this);
      }

      var client = new loop.shared.Client({
        baseServerUrl: options.baseServerUrl
      });

      function handleResult(err, sessionData) {
        /*jshint validthis:true */
        if (err) {
          this.trigger("session:error", new Error(
            "Retrieval of session information failed: HTTP " + err));
          return;
        }

        // XXX For incoming calls we might have more than one call queued.
        // For now, we'll just assume the first call is the right information.
        // We'll probably really want to be getting this data from the
        // background worker on the desktop client.
        // Bug 990714 should fix this.
        if (!options.outgoing)
          sessionData = sessionData[0];

        this.setReady(sessionData);
      }

      if (options.outgoing) {
        client.requestCallInfo(this.get("loopToken"), handleResult.bind(this));
      }
      else {
        client.requestCallsInfo(this.get("loopVersion"),
          handleResult.bind(this));
      }
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
