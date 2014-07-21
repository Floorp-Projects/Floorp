/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false, esnext:true */
/* global loop:true, React */

var loop = loop || {};
loop.conversation = (function(OT, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views,
      // aliasing translation function as __ for concision
      __ = mozL10n.get;

  /**
   * App router.
   * @type {loop.desktopRouter.DesktopConversationRouter}
   */
  var router;

  var IncomingCallView = React.createClass({displayName: 'IncomingCallView',

    propTypes: {
      model: React.PropTypes.func.isRequired
    },

    /**
     * Used for adding different styles to the panel
     * @returns {String} Corresponds to the client platform
     * */
    _getTargetPlatform: function() {
      var platform="unknown_platform";

      if (navigator.platform.indexOf("Win") !== -1) {
        platform = "windows";
      }
      if (navigator.platform.indexOf("Mac") !== -1) {
        platform = "mac";
      }
      if (navigator.platform.indexOf("Linux") !== -1) {
        platform = "linux";
      }

      return platform;
    },

    _handleAccept: function() {
      this.props.model.trigger("accept");
    },

    _handleDecline: function() {
      this.props.model.trigger("decline");
    },

    render: function() {
      /* jshint ignore:start */
      var btnClassAccept = "btn btn-error btn-decline";
      var btnClassDecline = "btn btn-success btn-accept";
      var conversationPanelClass = "incoming-call " + this._getTargetPlatform();
      return (
        React.DOM.div( {className:conversationPanelClass}, 
          React.DOM.h2(null, __("incoming_call")),
          React.DOM.div( {className:"button-group"}, 
            React.DOM.button( {className:btnClassAccept, onClick:this._handleDecline}, 
              __("incoming_call_decline_button")
            ),
            React.DOM.button( {className:btnClassDecline, onClick:this._handleAccept}, 
              __("incoming_call_answer_button")
            )
          )
        )
      );
      /* jshint ignore:end */
    }
  });

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
  var ConversationRouter = loop.desktopRouter.DesktopConversationRouter.extend({
    routes: {
      "incoming/:version": "incoming",
      "call/accept": "accept",
      "call/decline": "decline",
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
     * Incoming call route.
     *
     * @param {String} loopVersion The version from the push notification, set
     *                             by the router from the URL.
     */
    incoming: function(loopVersion) {
      window.navigator.mozLoop.startAlerting();
      this._conversation.set({loopVersion: loopVersion});
      this._conversation.once("accept", function() {
        this.navigate("call/accept", {trigger: true});
      }.bind(this));
      this._conversation.once("decline", function() {
        this.navigate("call/decline", {trigger: true});
      }.bind(this));
      this.loadReactComponent(loop.conversation.IncomingCallView({
        model: this._conversation
      }));
    },

    /**
     * Accepts an incoming call.
     */
    accept: function() {
      window.navigator.mozLoop.stopAlerting();
      this._conversation.initiate({
        client: new loop.Client(),
        outgoing: false
      });
    },

    /**
     * Declines an incoming call.
     */
    decline: function() {
      window.navigator.mozLoop.stopAlerting();
      // XXX For now, we just close the window
      window.close();
    },

    /**
     * conversation is the route when the conversation is active. The start
     * route should be navigated to first.
     */
    conversation: function() {
      if (!this._conversation.isSessionReady()) {
        console.error("Error: navigated to conversation route without " +
          "the start route to initialise the call first");
        this._notifier.errorL10n("cannot_start_call_session_not_ready");
        return;
      }

      /*jshint newcap:false*/
      this.loadReactComponent(sharedViews.ConversationView({
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
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(window.navigator.mozLoop);

    document.title = mozL10n.get("incoming_call_title");

    router = new ConversationRouter({
      conversation: new loop.shared.models.ConversationModel({}, {sdk: OT}),
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();
  }

  return {
    ConversationRouter: ConversationRouter,
    EndedCallView: EndedCallView,
    IncomingCallView: IncomingCallView,
    init: init
  };
})(window.OT, document.mozL10n);
