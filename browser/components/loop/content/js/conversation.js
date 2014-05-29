/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

Components.utils.import("resource://gre/modules/Services.jsm");

var loop = loop || {};
loop.conversation = (function(TB, mozl10n) {
  "use strict";

  var baseServerUrl = Services.prefs.getCharPref("loop.server"),
      // aliasing translation function as __ for concision
      __ = mozl10n.get;

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
    _conversation: undefined,
    _notifier:     undefined,
    activeView:    undefined,

    routes: {
      "start/:version": "start",
      "call/ongoing": "conversation",
      "call/ended": "ended"
    },

    /**
     * Loads and render current active view.
     *
     * @param {loop.shared.BaseView} view View.
     */
    loadView : function(view) {
      if (this.activeView) {
        this.activeView.hide();
      }
      this.activeView = view.render().show();
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
    },

    /**
     * Navigates to conversation when the call session is ready.
     */
    _onSessionReady: function() {
      this.navigate("call/ongoing", {trigger: true});
    },

    /**
     * Navigates to ended state when the call has ended
     */
    _onSessionEnded: function() {
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
        // XXX: notify user that something has gone wrong.
        console.error("Error: navigated to conversation route without " +
          "the start route to initialise the call first");
        this._notifier.notify(__("cannot_start_call_session_not_ready"));
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
      notifier: new loop.shared.views.NotificationListView({
        el: "#messages"
      })
    });
    Backbone.history.start();
  }

  return {
    ConversationRouter: ConversationRouter,
    init: init
  };
})(window.TB, document.mozL10n);
