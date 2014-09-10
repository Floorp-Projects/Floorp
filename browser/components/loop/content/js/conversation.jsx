/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false, esnext:true */
/* global loop:true, React */

var loop = loop || {};
loop.conversation = (function(OT, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views;

  /**
   * App router.
   * @type {loop.desktopRouter.DesktopConversationRouter}
   */
  var router;

  var IncomingCallView = React.createClass({

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
        <div className={conversationPanelClass}>
          <h2>{mozL10n.get("incoming_call_title2")}</h2>
          <div className="btn-group incoming-call-action-group">

            <div className="fx-embedded-incoming-call-button-spacer"></div>

            <div className="btn-chevron-menu-group">
              <div className="btn-group-chevron">
                <div className="btn-group">

                  <button className={btnClassDecline}
                          onClick={this._handleDecline}>
                    {mozL10n.get("incoming_call_cancel_button")}
                  </button>
                  <div className="btn-chevron"
                       onClick={this._toggleDeclineMenu}>
                  </div>
                </div>

                <ul className={dropdownMenuClassesDecline}>
                  <li className="btn-block" onClick={this._handleDeclineBlock}>
                    {mozL10n.get("incoming_call_cancel_and_block_button")}
                  </li>
                </ul>

              </div>
            </div>

            <div className="fx-embedded-incoming-call-button-spacer"></div>

            <AcceptCallButton mode={this._answerModeProps()} />

            <div className="fx-embedded-incoming-call-button-spacer"></div>

          </div>
        </div>
      );
      /* jshint ignore:end */
    }
  });

  /**
   * Incoming call view accept button, renders different primary actions
   * (answer with video / with audio only) based on the props received
   **/
  var AcceptCallButton = React.createClass({

    propTypes: {
      mode: React.PropTypes.object.isRequired,
    },

    render: function() {
      var mode = this.props.mode;
      return (
        /* jshint ignore:start */
        <div className="btn-chevron-menu-group">
          <div className="btn-group">
            <button className="btn btn-accept"
                    onClick={mode.primary.handler}
                    title={mozL10n.get(mode.primary.tooltip)}>
              <span className="fx-embedded-answer-btn-text">
                {mozL10n.get("incoming_call_accept_button")}
              </span>
              <span className={mode.primary.className}></span>
            </button>
            <div className={mode.secondary.className}
                 onClick={mode.secondary.handler}
                 title={mozL10n.get(mode.secondary.tooltip)}>
            </div>
          </div>
        </div>
        /* jshint ignore:end */
      );
    }
  });

  /**
   * Conversation router.
   *
   * Required options:
   * - {loop.shared.models.ConversationModel} conversation Conversation model.
   * - {loop.shared.models.NotificationCollection} notifications
   *
   * @type {loop.shared.router.BaseConversationRouter}
   */
  var ConversationRouter = loop.desktopRouter.DesktopConversationRouter.extend({
    routes: {
      "incoming/:callId": "incoming",
      "call/accept": "accept",
      "call/decline": "decline",
      "call/ongoing": "conversation",
      "call/declineAndBlock": "declineAndBlock",
      "call/feedback": "feedback"
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
      this.navigate("call/feedback", {trigger: true});
    },

    /**
     * Incoming call route.
     *
     * @param {String} callId  Identifier assigned by the LoopService
     *                         to this incoming call.
     */
    incoming: function(callId) {
      navigator.mozLoop.startAlerting();
      this._conversation.once("accept", function() {
        this.navigate("call/accept", {trigger: true});
      }.bind(this));
      this._conversation.once("decline", function() {
        this.navigate("call/decline", {trigger: true});
      }.bind(this));
      this._conversation.once("declineAndBlock", function() {
        this.navigate("call/declineAndBlock", {trigger: true});
      }.bind(this));
      this._conversation.once("call:incoming", this.startCall, this);
      this._conversation.once("change:publishedStream", this._checkConnected, this);
      this._conversation.once("change:subscribedStream", this._checkConnected, this);

      var callData = navigator.mozLoop.getCallData(callId);
      if (!callData) {
        console.error("Failed to get the call data");
        // XXX Not the ideal response, but bug 1047410 will be replacing
        // this by better "call failed" UI.
        this._notifications.errorL10n("cannot_start_call_session_not_ready");
        return;
      }
      this._conversation.setIncomingSessionData(callData);
      this._setupWebSocketAndCallView();
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
          model: this._conversation,
          video: this._conversation.hasVideoStream("incoming")
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
      navigator.mozLoop.stopAlerting();
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
      var token = this._conversation.get("callToken");
      this._client.deleteCallUrl(token, function(error) {
        // XXX The conversation window will be closed when this cb is triggered
        // figure out if there is a better way to report the error to the user
        // (bug 1048909).
        console.log(error);
      });
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

      var callType = this._conversation.get("selectedCallType");
      var videoStream = callType === "audio" ? false : true;

      /*jshint newcap:false*/
      this.loadReactComponent(sharedViews.ConversationView({
        sdk: OT,
        model: this._conversation,
        video: {enabled: videoStream}
      }));
    },

    /**
     * Handles a error starting the session
     */
    _handleSessionError: function() {
      // XXX Not the ideal response, but bug 1047410 will be replacing
      // this by better "call failed" UI.
      this._notifications.errorL10n("cannot_start_call_session_not_ready");
    },

    /**
     * Call has ended, display a feedback form.
     */
    feedback: function() {
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

      this.loadReactComponent(sharedViews.FeedbackView({
        feedbackApiClient: feedbackClient
      }));
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    document.title = mozL10n.get("incoming_call_title2");

    document.body.classList.add(loop.shared.utils.getTargetPlatform());

    var client = new loop.Client();
    router = new ConversationRouter({
      client: client,
      conversation: new loop.shared.models.ConversationModel(
        {},         // Model attributes
        {sdk: OT}), // Model dependencies
      notifications: new loop.shared.models.NotificationCollection()
    });
    Backbone.history.start();
  }

  return {
    ConversationRouter: ConversationRouter,
    IncomingCallView: IncomingCallView,
    init: init
  };
})(window.OT, document.mozL10n);
