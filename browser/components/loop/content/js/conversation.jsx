/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false, esnext:true */
/* global loop:true, React */

var loop = loop || {};
loop.conversation = (function(mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views;
  var sharedMixins = loop.shared.mixins;
  var sharedModels = loop.shared.models;
  var sharedActions = loop.shared.actions;

  var OutgoingConversationView = loop.conversationViews.OutgoingConversationView;
  var CallIdentifierView = loop.conversationViews.CallIdentifierView;
  var EmptyRoomView = loop.roomViews.EmptyRoomView;

  var IncomingCallView = React.createClass({
    mixins: [sharedMixins.DropdownMenuMixin],

    propTypes: {
      model: React.PropTypes.object.isRequired,
      video: React.PropTypes.bool.isRequired
    },

    getDefaultProps: function() {
      return {
        showMenu: false,
        video: true
      };
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
      var dropdownMenuClassesDecline = React.addons.classSet({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "visually-hidden": !this.state.showMenu
      });

      return (
        <div className="call-window">
          <CallIdentifierView video={this.props.video}
            peerIdentifier={this.props.model.getCallIdentifier()}
            urlCreationDate={this.props.model.get("urlCreationDate")}
            showIcons={true} />

          <div className="btn-group call-action-group">

            <div className="fx-embedded-call-button-spacer"></div>

            <div className="btn-chevron-menu-group">
              <div className="btn-group-chevron">
                <div className="btn-group">

                  <button className="btn btn-decline"
                          onClick={this._handleDecline}>
                    {mozL10n.get("incoming_call_cancel_button")}
                  </button>
                  <div className="btn-chevron" onClick={this.toggleDropdownMenu} />
                </div>

                <ul className={dropdownMenuClassesDecline}>
                  <li className="btn-block" onClick={this._handleDeclineBlock}>
                    {mozL10n.get("incoming_call_cancel_and_block_button")}
                  </li>
                </ul>

              </div>
            </div>

            <div className="fx-embedded-call-button-spacer"></div>

            <AcceptCallButton mode={this._answerModeProps()} />

            <div className="fx-embedded-call-button-spacer"></div>

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
   * Incoming Call failed view. Displayed when a call fails.
   *
   * XXX Based on CallFailedView, but built specially until we flux-ify the
   * incoming call views (bug 1088672).
   */
  var IncomingCallFailedView = React.createClass({
    propTypes: {
      cancelCall: React.PropTypes.func.isRequired
    },

    render: function() {
      document.title = mozL10n.get("generic_failure_title");

      return (
        <div className="call-window">
          <h2>{mozL10n.get("generic_failure_title")}</h2>

          <div className="btn-group call-action-group">
            <button className="btn btn-cancel"
                    onClick={this.props.cancelCall}>
              {mozL10n.get("cancel_button")}
            </button>
          </div>
        </div>
      );
    }
  });

  /**
   * This view manages the incoming conversation views - from
   * call initiation through to the actual conversation and call end.
   *
   * At the moment, it does more than that, these parts need refactoring out.
   */
  var IncomingConversationView = React.createClass({
    propTypes: {
      client: React.PropTypes.instanceOf(loop.Client).isRequired,
      conversation: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                         .isRequired,
      sdk: React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return {
        callFailed: false, // XXX this should be removed when bug 1047410 lands.
        callStatus: "start"
      };
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
            <IncomingCallView
              model={this.props.conversation}
              video={this.props.conversation.hasVideoStream("incoming")}
            />
          );
        }
        case "connected": {
          document.title = this.props.conversation.getCallIdentifier();

          var callType = this.props.conversation.get("selectedCallType");

          return (
            <sharedViews.ConversationView
              initiate={true}
              sdk={this.props.sdk}
              model={this.props.conversation}
              video={{enabled: callType !== "audio"}}
            />
          );
        }
        case "end": {
          // XXX To be handled with the "failed" view state when bug 1047410 lands
          if (this.state.callFailed) {
            return <IncomingCallFailedView
              cancelCall={this.closeWindow.bind(this)}
            />
          }

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
            <sharedViews.FeedbackView
              feedbackApiClient={feedbackClient}
              onAfterFeedbackReceived={this.closeWindow.bind(this)}
            />
          );
        }
        case "close": {
          window.close();
          return (<div/>);
        }
      }
    },

    /**
     * Notify the user that the connection was not possible
     * @param {{code: number, message: string}} error
     */
    _notifyError: function(error) {
      // XXX Not the ideal response, but bug 1047410 will be replacing
      // this by better "call failed" UI.
      console.error(error);
      this.setState({callFailed: true, callStatus: "end"});
    },

    /**
     * Peer hung up. Notifies the user and ends the call.
     *
     * Event properties:
     * - {String} connectionId: OT session id
     */
    _onPeerHungup: function() {
      this.setState({callFailed: false, callStatus: "end"});
    },

    /**
     * Network disconnected. Notifies the user and ends the call.
     */
    _onNetworkDisconnected: function() {
      // XXX Not the ideal response, but bug 1047410 will be replacing
      // this by better "call failed" UI.
      this.setState({callFailed: true, callStatus: "end"});
    },

    /**
     * Incoming call route.
     */
    setupIncomingCall: function() {
      navigator.mozLoop.startAlerting();

      var callData = navigator.mozLoop.getCallData(this.props.conversation.get("callId"));
      if (!callData) {
        // XXX Not the ideal response, but bug 1047410 will be replacing
        // this by better "call failed" UI.
        console.error("Failed to get the call data");
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
      this._websocket.promiseConnect().then(function(progressStatus) {
        this.setState({
          callStatus: progressStatus === "terminated" ? "close" : "incoming"
        });
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

      // XXX This would be nicer in the _abortIncomingCall function, but we need to stop
      // it here for now due to server-side issues that are being fixed in bug 1088351.
      // This is before the abort call to ensure that it happens before the window is
      // closed.
      navigator.mozLoop.stopAlerting();

      // If we hit any of the termination reasons, and the user hasn't accepted
      // then it seems reasonable to close the window/abort the incoming call.
      //
      // If the user has accepted the call, and something's happened, display
      // the call failed view.
      //
      // https://wiki.mozilla.org/Loop/Architecture/MVP#Termination_Reasons
      if (previousState === "init" || previousState === "alerting") {
        this._abortIncomingCall();
      } else {
        this.setState({callFailed: true, callStatus: "end"});
      }

    },

    /**
     * Silently aborts an incoming call - stops the alerting, and
     * closes the websocket.
     */
    _abortIncomingCall: function() {
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
      this.props.client.deleteCallUrl(token,
        this.props.conversation.get("sessionType"),
        function(error) {
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
      console.error("Failed initiating the call session.");
    },
  });

  /**
   * Master controller view for handling if incoming or outgoing calls are
   * in progress, and hence, which view to display.
   */
  var AppControllerView = React.createClass({
    propTypes: {
      // XXX Old types required for incoming call view.
      client: React.PropTypes.instanceOf(loop.Client).isRequired,
      conversation: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                         .isRequired,
      sdk: React.PropTypes.object.isRequired,

      // XXX New types for OutgoingConversationView
      store: React.PropTypes.instanceOf(loop.store.ConversationStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,

      // if not passed, this is not a room view
      localRoomStore: React.PropTypes.instanceOf(loop.store.LocalRoomStore)
    },

    getInitialState: function() {
      return this.props.store.attributes;
    },

    componentWillMount: function() {
      this.props.store.on("change:outgoing", function() {
        this.setState(this.props.store.attributes);
      }, this);
    },

    render: function() {
      if (this.props.localRoomStore) {
        return (
          <EmptyRoomView
            mozLoop={navigator.mozLoop}
            localRoomStore={this.props.localRoomStore}
          />
        );
      }

      // Don't display anything, until we know what type of call we are.
      if (this.state.outgoing === undefined) {
        return null;
      }

      if (this.state.outgoing) {
        return (<OutgoingConversationView
          store={this.props.store}
          dispatcher={this.props.dispatcher}
        />);
      }

      return (<IncomingConversationView
        client={this.props.client}
        conversation={this.props.conversation}
        sdk={this.props.sdk}
      />);
    }
  });

  /**
   * Conversation initialisation.
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

    var dispatcher = new loop.Dispatcher();
    var client = new loop.Client();
    var sdkDriver = new loop.OTSdkDriver({
      dispatcher: dispatcher,
      sdk: OT
    });

    var conversationStore = new loop.store.ConversationStore({}, {
      client: client,
      dispatcher: dispatcher,
      sdkDriver: sdkDriver
    });

    // XXX Old class creation for the incoming conversation view, whilst
    // we transition across (bug 1072323).
    var conversation = new sharedModels.ConversationModel(
      {},                // Model attributes
      {sdk: window.OT}   // Model dependencies
    );

    // Obtain the callId and pass it through
    var helper = new loop.shared.utils.Helper();
    var locationHash = helper.locationData().hash;
    var callId;
    var outgoing;
    var localRoomStore;

    // XXX removeMe, along with noisy comment at the beginning of
    // conversation_test.js "when locationHash begins with #room".
    if (navigator.mozLoop.getLoopBoolPref("test.alwaysUseRooms")) {
      locationHash = "#room/32";
    }

    var hash = locationHash.match(/#incoming\/(.*)/);
    if (hash) {
      callId = hash[1];
      outgoing = false;
    } else if (hash = locationHash.match(/#room\/(.*)/)) {
      localRoomStore = new loop.store.LocalRoomStore({
        dispatcher: dispatcher,
        mozLoop: navigator.mozLoop
      });
    } else {
      hash = locationHash.match(/#outgoing\/(.*)/);
      if (hash) {
        callId = hash[1];
        outgoing = true;
      }
    }

    conversation.set({callId: callId});

    window.addEventListener("unload", function(event) {
      // Handle direct close of dialog box via [x] control.
      navigator.mozLoop.releaseCallData(callId);
    });

    React.renderComponent(<AppControllerView
      localRoomStore={localRoomStore}
      store={conversationStore}
      client={client}
      conversation={conversation}
      dispatcher={dispatcher}
      sdk={window.OT}
    />, document.querySelector('#main'));

   if (localRoomStore) {
      dispatcher.dispatch(
        new sharedActions.SetupEmptyRoom({localRoomId: hash[1]}));
      return;
    }

    dispatcher.dispatch(new loop.shared.actions.GatherCallData({
      callId: callId,
      outgoing: outgoing
    }));
  }

  return {
    AppControllerView: AppControllerView,
    IncomingConversationView: IncomingConversationView,
    IncomingCallView: IncomingCallView,
    IncomingCallFailedView: IncomingCallFailedView,
    init: init
  };
})(document.mozL10n);

document.addEventListener('DOMContentLoaded', loop.conversation.init);
