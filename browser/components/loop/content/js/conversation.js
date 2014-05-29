/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

Components.utils.import("resource://gre/modules/Services.jsm");

var loop = loop || {};
loop.conversation = (function(OT, mozL10n) {
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
   * Call ended view.
   * @type {loop.shared.views.BaseView}
   */
  var EndedCallView = sharedViews.BaseView.extend({
    template: _.template([
      '<p>',
      '  <button class="btn btn-info" data-l10n-id="close_window"></button>',
      '</p>'
    ].join("")),

    className: "call-ended",

    events: {
      "click button": "closeWindow"
    },

    closeWindow: function(event) {
      event.preventDefault();
      // XXX For now, we just close the window.
      window.close();
    }
  });

  /**
   * Conversation router.
   *
   * Required options:
   * - {loop.shared.models.ConversationModel} conversation Conversation model.
   * - {loop.shared.components.Notifier}      notifier     Notifier component.
   *
   * @type {loop.shared.router.BaseConversationRouter}
   */
  var ConversationRouter = loop.shared.router.BaseConversationRouter.extend({
    routes: {
      "start/:version": "start",
      "call/ongoing": "conversation",
      "call/ended": "ended"
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.startCall}
     */
    startCall: function() {
      this.navigate("call/ongoing", {trigger: true});
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.endCall}
     */
    endCall: function() {
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
          sdk: OT,
          model: this._conversation
      }));
    },

    /**
     * XXX: load a view with a close button for now?
     */
    ended: function() {
      this.loadView(new EndedCallView());
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    router = new ConversationRouter({
      conversation: new loop.shared.models.ConversationModel({}, {sdk: OT}),
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();
  }

  return {
    ConversationRouter: ConversationRouter,
    EndedCallView: EndedCallView,
    init: init
  };
})(window.OT, document.mozL10n);
