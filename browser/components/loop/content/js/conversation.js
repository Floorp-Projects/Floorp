/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false, esnext:true */
/* global loop:true, React */

var loop = loop || {};
loop.conversation = (function(mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views,
      sharedModels = loop.shared.models;

  var IncomingCallView = React.createClass({displayName: 'IncomingCallView',

    propTypes: {
      model: React.PropTypes.object.isRequired,
      video: React.PropTypes.bool.isRequired
    },

    getDefaultProps: function() {
      return {
        showDeclineMenu: false,
        video: true
      };
    },

    getInitialState: function() {
      return {showDeclineMenu: this.props.showDeclineMenu};
    },

    componentDidMount: function() {
      window.addEventListener("click", this.clickHandler);
      window.addEventListener("blur", this._hideDeclineMenu);
    },

    componentWillUnmount: function() {
      window.removeEventListener("click", this.clickHandler);
      window.removeEventListener("blur", this._hideDeclineMenu);
    },

    clickHandler: function(e) {
      var target = e.target;
      if (!target.classList.contains('btn-chevron')) {
        this._hideDeclineMenu();
      }
    },

    _handleAccept: function(callType) {
      return function() {
        this.props.model.set("selectedCallType", callType);
        this.props.model.trigger("accept");
      }.bind(this);
    },

    _handleDecline: function() {
      this.props.model.trigger("decline");
    },

    _handleDeclineBlock: function(e) {
      this.props.model.trigger("declineAndBlock");
      /* Prevent event propagation
       * stop the click from reaching parent element */
      return false;
    },

    _toggleDeclineMenu: function() {
      var currentState = this.state.showDeclineMenu;
      this.setState({showDeclineMenu: !currentState});
    },

    _hideDeclineMenu: function() {
      this.setState({showDeclineMenu: false});
    },

    /*
     * Generate props for <AcceptCallButton> component based on
     * incoming call type. An incoming video call will render a video
     * answer button primarily, an audio call will flip them.
     **/
    _answerModeProps: function() {
      var videoButton = {
        handler: this._handleAccept("audio-video"),
        className: "fx-embedded-btn-icon-video",
        tooltip: "incoming_call_accept_audio_video_tooltip"
      };
      var audioButton = {
        handler: this._handleAccept("audio"),
        className: "fx-embedded-btn-audio-small",
        tooltip: "incoming_call_accept_audio_only_tooltip"
      };
      var props = {};
      props.primary = videoButton;
      props.secondary = audioButton;

      // When video is not enabled on this call, we swap the buttons around.
      if (!this.props.video) {
        audioButton.className = "fx-embedded-btn-icon-audio";
        videoButton.className = "fx-embedded-btn-video-small";
        props.primary = audioButton;
        props.secondary = videoButton;
      }

      return props;
    },

    render: function() {
      /* jshint ignore:start */
      var btnClassAccept = "btn btn-accept";
      var btnClassDecline = "btn btn-error btn-decline";
      var conversationPanelClass = "incoming-call";
      var dropdownMenuClassesDecline = React.addons.classSet({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "visually-hidden": !this.state.showDeclineMenu
      });
      return (
        React.DOM.div({className: conversationPanelClass}, 
          React.DOM.h2(null, mozL10n.get("incoming_call_title2")), 
          React.DOM.div({className: "btn-group incoming-call-action-group"}, 

            React.DOM.div({className: "fx-embedded-incoming-call-button-spacer"}), 

            React.DOM.div({className: "btn-chevron-menu-group"}, 
              React.DOM.div({className: "btn-group-chevron"}, 
                React.DOM.div({className: "btn-group"}, 

                  React.DOM.button({className: btnClassDecline, 
                          onClick: this._handleDecline}, 
                    mozL10n.get("incoming_call_cancel_button")
                  ), 
                  React.DOM.div({className: "btn-chevron", 
                       onClick: this._toggleDeclineMenu}
                  )
                ), 

                React.DOM.ul({className: dropdownMenuClassesDecline}, 
                  React.DOM.li({className: "btn-block", onClick: this._handleDeclineBlock}, 
                    mozL10n.get("incoming_call_cancel_and_block_button")
                  )
                )

              )
            ), 

            React.DOM.div({className: "fx-embedded-incoming-call-button-spacer"}), 

            AcceptCallButton({mode: this._answerModeProps()}), 

            React.DOM.div({className: "fx-embedded-incoming-call-button-spacer"})

          )
        )
      );
      /* jshint ignore:end */
    }
  });

  /**
   * Incoming call view accept button, renders different primary actions
   * (answer with video / with audio only) based on the props received
   **/
  var AcceptCallButton = React.createClass({displayName: 'AcceptCallButton',

    propTypes: {
      mode: React.PropTypes.object.isRequired,
    },

    render: function() {
      var mode = this.props.mode;
      return (
        /* jshint ignore:start */
        React.DOM.div({className: "btn-chevron-menu-group"}, 
          React.DOM.div({className: "btn-group"}, 
            React.DOM.button({className: "btn btn-accept", 
                    onClick: mode.primary.handler, 
                    title: mozL10n.get(mode.primary.tooltip)}, 
              React.DOM.span({className: "fx-embedded-answer-btn-text"}, 
                mozL10n.get("incoming_call_accept_button")
              ), 
              React.DOM.span({className: mode.primary.className})
            ), 
            React.DOM.div({className: mode.secondary.className, 
                 onClick: mode.secondary.handler, 
                 title: mozL10n.get(mode.secondary.tooltip)}
            )
          )
        )
        /* jshint ignore:end */
      );
    }
  });

  /**
   * This view manages the incoming conversation views - from
   * call initiation through to the actual conversation and call end.
   *
   * At the moment, it does more than that, these parts need refactoring out.
   */
  var IncomingConversationView = React.createClass({displayName: 'IncomingConversationView',
    propTypes: {
      client: React.PropTypes.instanceOf(loop.Client).isRequired,
      conversation: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                         .isRequired,
      notifications: React.PropTypes.instanceOf(sharedModels.NotificationCollection)
                          .isRequired,
      sdk: React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return {
        callStatus: "start"
      }
    },

    componentDidMount: function() {
      this.props.conversation.on("accept", this.accept, this);
      this.props.conversation.on("decline", this.decline, this);
      this.props.conversation.on("declineAndBlock", this.declineAndBlock, this);
      this.props.conversation.on("call:accepted", this.accepted, this);
      this.props.conversation.on("change:publishedStream", this._checkConnected, this);
      this.props.conversation.on("change:subscribedStream", this._checkConnected, this);
      this.props.conversation.on("session:ended", this.endCall, this);
      this.props.conversation.on("session:peer-hungup", this._onPeerHungup, this);
      this.props.conversation.on("session:network-disconnected", this._onNetworkDisconnected, this);
      this.props.conversation.on("session:connection-error", this._notifyError, this);

      this.setupIncomingCall();
    },

    componentDidUnmount: function() {
      this.props.conversation.off(null, null, this);
    },

    render: function() {
      switch (this.state.callStatus) {
        case "start": {
          document.title = mozL10n.get("incoming_call_title2");

          // XXX Don't render anything initially, though this should probably
          // be some sort of pending view, whilst we connect the websocket.
          return null;
        }
        case "incoming": {
          document.title = mozL10n.get("incoming_call_title2");

          return (
            IncomingCallView({
              model: this.props.conversation, 
              video: this.props.conversation.hasVideoStream("incoming")}
            )
          );
        }
        case "connected": {
          // XXX This should be the caller id (bug 1020449)
          document.title = mozL10n.get("incoming_call_title2");

          var callType = this.props.conversation.get("selectedCallType");

          return (
            sharedViews.ConversationView({
              initiate: true, 
              sdk: this.props.sdk, 
              model: this.props.conversation, 
              video: {enabled: callType !== "audio"}}
            )
          );
        }
        case "end": {
          document.title = mozL10n.get("conversation_has_ended");

          var feebackAPIBaseUrl = navigator.mozLoop.getLoopCharPref(
            "feedback.baseUrl");

          var appVersionInfo = navigator.mozLoop.appVersionInfo;

          var feedbackClient = new loop.FeedbackAPIClient(feebackAPIBaseUrl, {
            product: navigator.mozLoop.getLoopCharPref("feedback.product"),
            platform: appVersionInfo.OS,
            channel: appVersionInfo.channel,
            version: appVersionInfo.version
          });

          return (
            sharedViews.FeedbackView({
              feedbackApiClient: feedbackClient, 
              onAfterFeedbackReceived: this.closeWindow.bind(this)}
            )
          );
        }
        case "close": {
          window.close();
          return (React.DOM.div(null));
        }
      }
    },

    /**
     * Notify the user that the connection was not possible
     * @param {{code: number, message: string}} error
     */
    _notifyError: function(error) {
      console.error(error);
      this.props.notifications.errorL10n("connection_error_see_console_notification");
      this.setState({callStatus: "end"});
    },

    /**
     * Peer hung up. Notifies the user and ends the call.
     *
     * Event properties:
     * - {String} connectionId: OT session id
     */
    _onPeerHungup: function() {
      this.props.notifications.warnL10n("peer_ended_conversation2");
      this.setState({callStatus: "end"});
    },

    /**
     * Network disconnected. Notifies the user and ends the call.
     */
    _onNetworkDisconnected: function() {
      this.props.notifications.warnL10n("network_disconnected");
      this.setState({callStatus: "end"});
    },

    /**
     * Incoming call route.
     */
    setupIncomingCall: function() {
      navigator.mozLoop.startAlerting();

      var callData = navigator.mozLoop.getCallData(this.props.conversation.get("callId"));
      if (!callData) {
        console.error("Failed to get the call data");
        // XXX Not the ideal response, but bug 1047410 will be replacing
        // this by better "call failed" UI.
        this.props.notifications.errorL10n("cannot_start_call_session_not_ready");
        return;
      }
      this.props.conversation.setIncomingSessionData(callData);
      this._setupWebSocket();
    },

    /**
     * Starts the actual conversation
     */
    accepted: function() {
      this.setState({callStatus: "connected"});
    },

    /**
     * Moves the call to the end state
     */
    endCall: function() {
      navigator.mozLoop.releaseCallData(this.props.conversation.get("callId"));
      this.setState({callStatus: "end"});
    },

    /**
     * Used to set up the web socket connection and navigate to the
     * call view if appropriate.
     */
    _setupWebSocket: function() {
      this._websocket = new loop.CallConnectionWebSocket({
        url: this.props.conversation.get("progressURL"),
        websocketToken: this.props.conversation.get("websocketToken"),
        callId: this.props.conversation.get("callId"),
      });
      this._websocket.promiseConnect().then(function() {
        this.setState({callStatus: "incoming"});
      }.bind(this), function() {
        this._handleSessionError();
        return;
      }.bind(this));

      this._websocket.on("progress", this._handleWebSocketProgress, this);
    },

    /**
     * Checks if the streams have been connected, and notifies the
     * websocket that the media is now connected.
     */
    _checkConnected: function() {
      // Check we've had both local and remote streams connected before
      // sending the media up message.
      if (this.props.conversation.streamsConnected()) {
        this._websocket.mediaUp();
      }
    },

    /**
     * Used to receive websocket progress and to determine how to handle
     * it if appropraite.
     * If we add more cases here, then we should refactor this function.
     *
     * @param {Object} progressData The progress data from the websocket.
     * @param {String} previousState The previous state from the websocket.
     */
    _handleWebSocketProgress: function(progressData, previousState) {
      // We only care about the terminated state at the moment.
      if (progressData.state !== "terminated")
        return;

      if (progressData.reason === "cancel") {
        this._abortIncomingCall();
        return;
      }

      if (progressData.reason === "timeout" &&
          (previousState === "init" || previousState === "alerting")) {
        this._abortIncomingCall();
      }
    },

    /**
     * Silently aborts an incoming call - stops the alerting, and
     * closes the websocket.
     */
    _abortIncomingCall: function() {
      navigator.mozLoop.stopAlerting();
      this._websocket.close();
      // Having a timeout here lets the logging for the websocket complete and be
      // displayed on the console if both are on.
      setTimeout(this.closeWindow, 0);
    },

    closeWindow: function() {
      window.close();
    },

    /**
     * Accepts an incoming call.
     */
    accept: function() {
      navigator.mozLoop.stopAlerting();
      this._websocket.accept();
      this.props.conversation.accepted();
    },

    /**
     * Declines a call and handles closing of the window.
     */
    _declineCall: function() {
      this._websocket.decline();
      navigator.mozLoop.releaseCallData(this.props.conversation.get("callId"));
      this._websocket.close();
      // Having a timeout here lets the logging for the websocket complete and be
      // displayed on the console if both are on.
      setTimeout(this.closeWindow, 0);
    },

    /**
     * Declines an incoming call.
     */
    decline: function() {
      navigator.mozLoop.stopAlerting();
      this._declineCall();
    },

    /**
     * Decline and block an incoming call
     * @note:
     * - loopToken is the callUrl identifier. It gets set in the panel
     *   after a callUrl is received
     */
    declineAndBlock: function() {
      navigator.mozLoop.stopAlerting();
      var token = this.props.conversation.get("callToken");
      this.props.client.deleteCallUrl(token, function(error) {
        // XXX The conversation window will be closed when this cb is triggered
        // figure out if there is a better way to report the error to the user
        // (bug 1048909).
        console.log(error);
      });
      this._declineCall();
    },

    /**
     * Handles a error starting the session
     */
    _handleSessionError: function() {
      // XXX Not the ideal response, but bug 1047410 will be replacing
      // this by better "call failed" UI.
      this.props.notifications.errorL10n("cannot_start_call_session_not_ready");
    },
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    // Plug in an alternate client ID mechanism, as localStorage and cookies
    // don't work in the conversation window
    window.OT.overrideGuidStorage({
      get: function(callback) {
        callback(null, navigator.mozLoop.getLoopCharPref("ot.guid"));
      },
      set: function(guid, callback) {
        navigator.mozLoop.setLoopCharPref("ot.guid", guid);
        callback(null);
      }
    });

    document.body.classList.add(loop.shared.utils.getTargetPlatform());

    var client = new loop.Client();
    var conversation = new sharedModels.ConversationModel(
      {},                // Model attributes
      {sdk: window.OT}   // Model dependencies
    );
    var notifications = new sharedModels.NotificationCollection();

    window.addEventListener("unload", function(event) {
      // Handle direct close of dialog box via [x] control.
      navigator.mozLoop.releaseCallData(conversation.get("callId"));
    });

    // Obtain the callId and pass it to the conversation
    var helper = new loop.shared.utils.Helper();
    var locationHash = helper.locationHash();
    if (locationHash) {
      conversation.set("callId", locationHash.match(/\#incoming\/(.*)/)[1]);
    }

    React.renderComponent(IncomingConversationView({
      client: client, 
      conversation: conversation, 
      notifications: notifications, 
      sdk: window.OT}
    ), document.querySelector('#main'));
  }

  return {
    IncomingConversationView: IncomingConversationView,
    IncomingCallView: IncomingCallView,
    init: init
  };
})(document.mozL10n);

document.addEventListener('DOMContentLoaded', loop.conversation.init);
