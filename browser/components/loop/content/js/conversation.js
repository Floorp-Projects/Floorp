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
      model: React.PropTypes.object.isRequired
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
        React.DOM.div({className: conversationPanelClass}, 
          React.DOM.h2(null, __("incoming_call")), 
          React.DOM.div({className: "button-group"}, 
            React.DOM.button({className: btnClassAccept, onClick: this._handleDecline}, 
              __("incoming_call_decline_button")
            ), 
            React.DOM.button({className: btnClassDecline, onClick: this._handleAccept}, 
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
      this._conversation.once("accept", () => {
        this.navigate("call/accept", {trigger: true});
      });
      this._conversation.once("decline", () => {
        this.navigate("call/decline", {trigger: true});
      });
      this._conversation.once("call:incoming", this.startCall, this);
      this._conversation.once("change:publishedStream", this._checkConnected, this);
      this._conversation.once("change:subscribedStream", this._checkConnected, this);

      this._client.requestCallsInfo(loopVersion, (err, sessionData) => {
        if (err) {
          console.error("Failed to get the sessionData", err);
          // XXX Not the ideal response, but bug 1047410 will be replacing
          // this by better "call failed" UI.
          this._notifier.errorL10n("cannot_start_call_session_not_ready");
          return;
        }
        // XXX For incoming calls we might have more than one call queued.
        // For now, we'll just assume the first call is the right information.
        // We'll probably really want to be getting this data from the
        // background worker on the desktop client.
        // Bug 1032700 should fix this.
        this._conversation.setSessionData(sessionData[0]);

        this._setupWebSocketAndCallView();
      });
    },

    /**
     * Used to set up the web socket connection and navigate to the
     * call view if appropriate.
     */
    _setupWebSocketAndCallView: function() {
      this._websocket = new loop.CallConnectionWebSocket({
        url: this._conversation.get("progressURL"),
        websocketToken: this._conversation.get("websocketToken"),
        callId: this._conversation.get("callId"),
      });
      this._websocket.promiseConnect().then(function() {
        this.loadReactComponent(loop.conversation.IncomingCallView({
          model: this._conversation
        }));
      }.bind(this), function() {
        this._handleSessionError();
        return;
      }.bind(this));
    },

    /**
     * Checks if the streams have been connected, and notifies the
     * websocket that the media is now connected.
     */
    _checkConnected: function() {
      // Check we've had both local and remote streams connected before
      // sending the media up message.
      if (this._conversation.streamsConnected()) {
        this._websocket.mediaUp();
      }
    },

    /**
     * Accepts an incoming call.
     */
    accept: function() {
      window.navigator.mozLoop.stopAlerting();
      this._websocket.accept();
      this._conversation.incoming();
    },

    /**
     * Declines a call and handles closing of the window.
     */
    _declineCall: function() {
      this._websocket.decline();
      // XXX Don't close the window straight away, but let any sends happen
      // first. Ideally we'd wait to close the window until after we have a
      // response from the server, to know that everything has completed
      // successfully. However, that's quite difficult to ensure at the
      // moment so we'll add it later.
      setTimeout(window.close, 0);
    },

    /**
     * Declines an incoming call.
     */
    decline: function() {
      window.navigator.mozLoop.stopAlerting();
      // XXX For now, we just close the window
      this._declineCall();
    },

    /**
     * conversation is the route when the conversation is active. The start
     * route should be navigated to first.
     */
    conversation: function() {
      if (!this._conversation.isSessionReady()) {
        console.error("Error: navigated to conversation route without " +
          "the start route to initialise the call first");
        this._handleSessionError();
        return;
      }

      /*jshint newcap:false*/
      this.loadReactComponent(sharedViews.ConversationView({
        sdk: OT,
        model: this._conversation
      }));
    },

    /**
     * Handles a error starting the session
     */
    _handleSessionError: function() {
      // XXX Not the ideal response, but bug 1047410 will be replacing
      // this by better "call failed" UI.
      this._notifier.errorL10n("cannot_start_call_session_not_ready");
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

    var client = new loop.Client();
    router = new ConversationRouter({
      client: client,
      conversation: new loop.shared.models.ConversationModel(
        {},         // Model attributes
        {sdk: OT}), // Model dependencies
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
