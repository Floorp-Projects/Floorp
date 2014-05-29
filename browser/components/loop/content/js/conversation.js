/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

Components.utils.import("resource://gre/modules/Services.jsm");

var loop = loop || {};
loop.conversation = (function(TB, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views,
      baseServerUrl = Services.prefs.getCharPref("loop.server"),
      // aliasing translation function as __ for concision
      __ = mozL10n.get;

  /**
   * App router.
   * @type {loop.webapp.Router}
   */
  var router;

  /**
   * Current conversation model instance.
   * @type {loop.webapp.ConversationModel}
   */
  var conversation;

  /**
   * Conversation router.
   *
   * Required options:
   * - {loop.shared.models.ConversationModel} conversation Conversation model.
   * - {loop.shared.components.Notifier}      notifier     Notifier component.
   */
  var ConversationRouter = loop.shared.router.BaseRouter.extend({
    /**
     * Current conversation.
     * @type {loop.shared.models.ConversationModel}
     */
    _conversation: undefined,

    /**
     * Notifications dispatcher.
     * @type {loop.shared.views.NotificationListView}
     */
    _notifier: undefined,

    routes: {
      "start/:version": "start",
      "call/ongoing": "conversation",
      "call/ended": "ended"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.conversation) {
        throw new Error("missing required conversation");
      }
      this._conversation = options.conversation;

      if (!options.notifier) {
        throw new Error("missing required notifier");
      }
      this._notifier = options.notifier;

      this.listenTo(this._conversation, "session:ready", this._onSessionReady);
      this.listenTo(this._conversation, "session:ended", this._onSessionEnded);
      this.listenTo(this._conversation, "session:peer-hung", this._onPeerHung);
      this.listenTo(this._conversation, "session:network-disconnected",
                                        this._onNetworkDisconnected);
    },

    /**
     * Navigates to conversation when the call session is ready.
     */
    _onSessionReady: function() {
      this.navigate("call/ongoing", {trigger: true});
    },

    /**
     * Navigates to ended state when the call has ended.
     */
    _onSessionEnded: function() {
      this.navigate("call/ended", {trigger: true});
    },

    /**
     * Peer hung up. Navigates back to call initiation so the user can start
     * calling again.
     *
     * Event properties:
     * - {String} connectionId: OT session id
     *
     * @param {Object} event
     */
    _onPeerHung: function(event) {
      this._notifier.warn(__("peer_ended_conversation"));
      this.navigate("call/ended", {trigger: true});
    },

    /**
     * Network disconnected. Navigates back to call initiation so the user can
     * start calling again.
     */
    _onNetworkDisconnected: function() {
      this._notifier.warn(__("network_disconnected"));
      this.navigate("call/ended", {trigger: true});
    },

    /**
     * start is the initial route that does any necessary prompting and set
     * up for the call.
     *
     * @param {String} loopVersion The version from the push notification, set
     *                             by the router from the URL.
     */
    start: function(loopVersion) {
      // XXX For now, we just kick the conversation straight away, bug 990678
      // will implement the follow-ups.
      this._conversation.set({loopVersion: loopVersion});
      this._conversation.initiate({
        baseServerUrl: baseServerUrl,
        outgoing: false
      });
    },

    /**
     * conversation is the route when the conversation is active. The start
     * route should be navigated to first.
     */
    conversation: function() {
      if (!this._conversation.isSessionReady()) {
        console.error("Error: navigated to conversation route without " +
          "the start route to initialise the call first");
        this._notifier.error(__("cannot_start_call_session_not_ready"));
        return;
      }

      this.loadView(
        new loop.shared.views.ConversationView({
          sdk: TB,
          model: this._conversation
      }));
    },

    /**
     * ended does any necessary work to end the call.
     */
    ended: function() {
      // XXX Later we implement the end-of call here (bug 974873)
      window.close();
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    conversation = new loop.shared.models.ConversationModel();
    router = new ConversationRouter({
      conversation: conversation,
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();
  }

  return {
    ConversationRouter: ConversationRouter,
    init: init
  };
})(window.TB, document.mozL10n);
