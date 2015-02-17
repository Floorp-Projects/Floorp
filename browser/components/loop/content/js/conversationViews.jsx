/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.conversationViews = (function(mozL10n) {

  var CALL_STATES = loop.store.CALL_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sharedViews = loop.shared.views;
  var sharedMixins = loop.shared.mixins;
  var sharedModels = loop.shared.models;

  // This duplicates a similar function in contacts.jsx that isn't used in the
  // conversation window. If we get too many of these, we might want to consider
  // finding a logical place for them to be shared.

  // XXXdmose this code is already out of sync with the code in contacts.jsx
  // which, unlike this code, now has unit tests.  We should totally do the
  // above.

  function _getPreferredEmail(contact) {
    // A contact may not contain email addresses, but only a phone number.
    if (!contact.email || contact.email.length === 0) {
      return { value: "" };
    }
    return contact.email.find(e => e.pref) || contact.email[0];
  }

  function _getContactDisplayName(contact) {
    if (contact.name && contact.name[0]) {
      return contact.name[0];
    }
    return _getPreferredEmail(contact).value;
  }

  /**
   * Displays information about the call
   * Caller avatar, name & conversation creation date
   */
  var CallIdentifierView = React.createClass({
    propTypes: {
      peerIdentifier: React.PropTypes.string,
      showIcons: React.PropTypes.bool.isRequired,
      urlCreationDate: React.PropTypes.string,
      video: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {
        peerIdentifier: "",
        showLinkDetail: true,
        urlCreationDate: "",
        video: true
      };
    },

    getInitialState: function() {
      return {timestamp: 0};
    },

    /**
     * Gets and formats the incoming call creation date
     */
    formatCreationDate: function() {
      if (!this.props.urlCreationDate) {
        return "";
      }

      var timestamp = this.props.urlCreationDate;
      return "(" + loop.shared.utils.formatDate(timestamp) + ")";
    },

    render: function() {
      var iconVideoClasses = React.addons.classSet({
        "fx-embedded-tiny-video-icon": true,
        "muted": !this.props.video
      });
      var callDetailClasses = React.addons.classSet({
        "fx-embedded-call-detail": true,
        "hide": !this.props.showIcons
      });

      return (
        <div className="fx-embedded-call-identifier">
          <div className="fx-embedded-call-identifier-avatar fx-embedded-call-identifier-item"/>
          <div className="fx-embedded-call-identifier-info fx-embedded-call-identifier-item">
            <div className="fx-embedded-call-identifier-text overflow-text-ellipsis">
              {this.props.peerIdentifier}
            </div>
            <div className={callDetailClasses}>
              <span className="fx-embedded-tiny-audio-icon"></span>
              <span className={iconVideoClasses}></span>
              <span className="fx-embedded-conversation-timestamp">
                {this.formatCreationDate()}
              </span>
            </div>
          </div>
        </div>
      );
    }
  });

  /**
   * Displays details of the incoming/outgoing conversation
   * (name, link, audio/video type etc).
   *
   * Allows the view to be extended with different buttons and progress
   * via children properties.
   */
  var ConversationDetailView = React.createClass({
    propTypes: {
      contact: React.PropTypes.object
    },

    render: function() {
      var contactName = _getContactDisplayName(this.props.contact);

      document.title = contactName;

      return (
        <div className="call-window">
          <CallIdentifierView
            peerIdentifier={contactName}
            showIcons={false} />
          <div>{this.props.children}</div>
        </div>
      );
    }
  });

  // Matches strings of the form "<nonspaces>@<nonspaces>" or "+<digits>"
  var EMAIL_OR_PHONE_RE = /^(:?\S+@\S+|\+\d+)$/;

  var IncomingCallView = React.createClass({
    mixins: [sharedMixins.DropdownMenuMixin, sharedMixins.AudioMixin],

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
   * Something went wrong view. Displayed when there's a big problem.
   *
   * XXX Based on CallFailedView, but built specially until we flux-ify the
   * incoming call views (bug 1088672).
   */
  var GenericFailureView = React.createClass({
    mixins: [sharedMixins.AudioMixin],

    propTypes: {
      cancelCall: React.PropTypes.func.isRequired
    },

    componentDidMount: function() {
      this.play("failure");
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
    mixins: [sharedMixins.AudioMixin, sharedMixins.WindowCloseMixin],

    propTypes: {
      client: React.PropTypes.instanceOf(loop.Client).isRequired,
      conversation: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                         .isRequired,
      sdk: React.PropTypes.object.isRequired,
      conversationAppStore: React.PropTypes.instanceOf(
        loop.store.ConversationAppStore).isRequired
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
            return <GenericFailureView
              cancelCall={this.closeWindow.bind(this)}
            />;
          }

          document.title = mozL10n.get("conversation_has_ended");

          this.play("terminated");

          return (
            <sharedViews.FeedbackView
              onAfterFeedbackReceived={this.closeWindow.bind(this)}
            />
          );
        }
        case "close": {
          this.closeWindow();
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

      // XXX This is a hack until we rework for the flux model in bug 1088672.
      var callData = this.props.conversationAppStore.getStoreState().windowData;

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
      navigator.mozLoop.calls.clearCallInProgress(
        this.props.conversation.get("windowId"));
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
      navigator.mozLoop.calls.clearCallInProgress(
        this.props.conversation.get("windowId"));
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
      var callerId = this.props.conversation.get("callerId");

      // If this is a direct call, we'll need to block the caller directly.
      if (callerId && EMAIL_OR_PHONE_RE.test(callerId)) {
        navigator.mozLoop.calls.blockDirectCaller(callerId, function(err) {
          // XXX The conversation window will be closed when this cb is triggered
          // figure out if there is a better way to report the error to the user
          // (bug 1103150).
          console.log(err.fileName + ":" + err.lineNumber + ": " + err.message);
        });
      } else {
        this.props.client.deleteCallUrl(token,
          this.props.conversation.get("sessionType"),
          function(error) {
            // XXX The conversation window will be closed when this cb is triggered
            // figure out if there is a better way to report the error to the user
            // (bug 1048909).
            console.log(error);
          });
      }

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
   * View for pending conversations. Displays a cancel button and appropriate
   * pending/ringing strings.
   */
  var PendingConversationView = React.createClass({
    mixins: [sharedMixins.AudioMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      callState: React.PropTypes.string,
      contact: React.PropTypes.object,
      enableCancelButton: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {
        enableCancelButton: false
      };
    },

    componentDidMount: function() {
      this.play("ringtone", {loop: true});
    },

    cancelCall: function() {
      this.props.dispatcher.dispatch(new sharedActions.CancelCall());
    },

    render: function() {
      var cx = React.addons.classSet;
      var pendingStateString;
      if (this.props.callState === CALL_STATES.ALERTING) {
        pendingStateString = mozL10n.get("call_progress_ringing_description");
      } else {
        pendingStateString = mozL10n.get("call_progress_connecting_description");
      }

      var btnCancelStyles = cx({
        "btn": true,
        "btn-cancel": true,
        "disabled": !this.props.enableCancelButton
      });

      return (
        <ConversationDetailView contact={this.props.contact}>

          <p className="btn-label">{pendingStateString}</p>

          <div className="btn-group call-action-group">
            <button className={btnCancelStyles}
                    onClick={this.cancelCall}>
              {mozL10n.get("initiate_call_cancel_button")}
            </button>
          </div>

        </ConversationDetailView>
      );
    }
  });

  /**
   * Call failed view. Displayed when a call fails.
   */
  var CallFailedView = React.createClass({
    mixins: [
      Backbone.Events,
      sharedMixins.AudioMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      store: React.PropTypes.instanceOf(
        loop.store.ConversationStore).isRequired,
      contact: React.PropTypes.object.isRequired,
      // This is used by the UI showcase.
      emailLinkError: React.PropTypes.bool,
    },

    getInitialState: function() {
      return {
        emailLinkError: this.props.emailLinkError,
        emailLinkButtonDisabled: false
      };
    },

    componentDidMount: function() {
      this.play("failure");
      this.listenTo(this.props.store, "change:emailLink",
                    this._onEmailLinkReceived);
      this.listenTo(this.props.store, "error:emailLink",
                    this._onEmailLinkError);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.store);
    },

    _onEmailLinkReceived: function() {
      var emailLink = this.props.store.getStoreState("emailLink");
      var contactEmail = _getPreferredEmail(this.props.contact).value;
      sharedUtils.composeCallUrlEmail(emailLink, contactEmail);
      this.closeWindow();
    },

    _onEmailLinkError: function() {
      this.setState({
        emailLinkError: true,
        emailLinkButtonDisabled: false
      });
    },

    _renderError: function() {
      if (!this.state.emailLinkError) {
        return;
      }
      return <p className="error">{mozL10n.get("unable_retrieve_url")}</p>;
    },

    _getTitleMessage: function() {
      var callStateReason =
        this.props.store.getStoreState("callStateReason");

      if (callStateReason === WEBSOCKET_REASONS.REJECT || callStateReason === WEBSOCKET_REASONS.BUSY ||
          callStateReason === REST_ERRNOS.USER_UNAVAILABLE) {
        var contactDisplayName = _getContactDisplayName(this.props.contact);
        if (contactDisplayName.length) {
          return mozL10n.get(
            "contact_unavailable_title",
            {"contactName": contactDisplayName});
        }

        return mozL10n.get("generic_contact_unavailable_title");
      } else {
        return mozL10n.get("generic_failure_title");
      }
    },

    retryCall: function() {
      this.props.dispatcher.dispatch(new sharedActions.RetryCall());
    },

    cancelCall: function() {
      this.props.dispatcher.dispatch(new sharedActions.CancelCall());
    },

    emailLink: function() {
      this.setState({
        emailLinkError: false,
        emailLinkButtonDisabled: true
      });

      this.props.dispatcher.dispatch(new sharedActions.FetchRoomEmailLink({
        roomOwner: navigator.mozLoop.userProfile.email,
        roomName: _getContactDisplayName(this.props.contact)
      }));
    },

    render: function() {
      return (
        <div className="call-window">
          <h2>{ this._getTitleMessage() }</h2>

          <p className="btn-label">{mozL10n.get("generic_failure_with_reason2")}</p>

          {this._renderError()}

          <div className="btn-group call-action-group">
            <button className="btn btn-cancel"
                    onClick={this.cancelCall}>
              {mozL10n.get("cancel_button")}
            </button>
            <button className="btn btn-info btn-retry"
                    onClick={this.retryCall}>
              {mozL10n.get("retry_call_button")}
            </button>
            <button className="btn btn-info btn-email"
                    onClick={this.emailLink}
                    disabled={this.state.emailLinkButtonDisabled}>
              {mozL10n.get("share_button2")}
            </button>
          </div>
        </div>
      );
    }
  });

  var OngoingConversationView = React.createClass({
    mixins: [
      sharedMixins.MediaSetupMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      video: React.PropTypes.object,
      audio: React.PropTypes.object
    },

    getDefaultProps: function() {
      return {
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true}
      };
    },

    componentDidMount: function() {
      // The SDK needs to know about the configuration and the elements to use
      // for display. So the best way seems to pass the information here - ideally
      // the sdk wouldn't need to know this, but we can't change that.
      this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
        publisherConfig: this.getDefaultPublisherConfig({
          publishVideo: this.props.video.enabled
        }),
        getLocalElementFunc: this._getElement.bind(this, ".local"),
        getRemoteElementFunc: this._getElement.bind(this, ".remote")
      }));
    },

    /**
     * Hangs up the call.
     */
    hangup: function() {
      this.props.dispatcher.dispatch(
        new sharedActions.HangupCall());
    },

    /**
     * Used to control publishing a stream - i.e. to mute a stream
     *
     * @param {String} type The type of stream, e.g. "audio" or "video".
     * @param {Boolean} enabled True to enable the stream, false otherwise.
     */
    publishStream: function(type, enabled) {
      this.props.dispatcher.dispatch(
        new sharedActions.SetMute({
          type: type,
          enabled: enabled
        }));
    },

    render: function() {
      var localStreamClasses = React.addons.classSet({
        local: true,
        "local-stream": true,
        "local-stream-audio": !this.props.video.enabled
      });

      return (
        <div className="video-layout-wrapper">
          <div className="conversation">
            <div className="media nested">
              <div className="video_wrapper remote_wrapper">
                <div className="video_inner remote focus-stream"></div>
              </div>
              <div className={localStreamClasses}></div>
            </div>
            <loop.shared.views.ConversationToolbar
              video={this.props.video}
              audio={this.props.audio}
              publishStream={this.publishStream}
              hangup={this.hangup} />
          </div>
        </div>
      );
    }
  });

  /**
   * Master View Controller for outgoing calls. This manages
   * the different views that need displaying.
   */
  var OutgoingConversationView = React.createClass({
    mixins: [
      sharedMixins.AudioMixin,
      Backbone.Events
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      store: React.PropTypes.instanceOf(
        loop.store.ConversationStore).isRequired
    },

    getInitialState: function() {
      return this.props.store.getStoreState();
    },

    componentWillMount: function() {
      this.listenTo(this.props.store, "change", function() {
        this.setState(this.props.store.getStoreState());
      }, this);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.store, "change", function() {
        this.setState(this.props.store.getStoreState());
      }, this);
    },

    _closeWindow: function() {
      window.close();
    },

    /**
     * Returns true if the call is in a cancellable state, during call setup.
     */
    _isCancellable: function() {
      return this.state.callState !== CALL_STATES.INIT &&
             this.state.callState !== CALL_STATES.GATHER;
    },

    /**
     * Used to setup and render the feedback view.
     */
    _renderFeedbackView: function() {
      document.title = mozL10n.get("conversation_has_ended");

      return (
        <sharedViews.FeedbackView
          onAfterFeedbackReceived={this._closeWindow.bind(this)}
        />
      );
    },

    render: function() {
      switch (this.state.callState) {
        case CALL_STATES.CLOSE: {
          this._closeWindow();
          return null;
        }
        case CALL_STATES.TERMINATED: {
          return (<CallFailedView
            dispatcher={this.props.dispatcher}
            store={this.props.store}
            contact={this.state.contact}
          />);
        }
        case CALL_STATES.ONGOING: {
          return (<OngoingConversationView
            dispatcher={this.props.dispatcher}
            video={{enabled: !this.state.videoMuted}}
            audio={{enabled: !this.state.audioMuted}}
            />
          );
        }
        case CALL_STATES.FINISHED: {
          this.play("terminated");
          return this._renderFeedbackView();
        }
        case CALL_STATES.INIT: {
          // We know what we are, but we haven't got the data yet.
          return null;
        }
        default: {
          return (<PendingConversationView
            dispatcher={this.props.dispatcher}
            callState={this.state.callState}
            contact={this.state.contact}
            enableCancelButton={this._isCancellable()}
          />);
        }
      }
    },
  });

  return {
    PendingConversationView: PendingConversationView,
    CallIdentifierView: CallIdentifierView,
    ConversationDetailView: ConversationDetailView,
    CallFailedView: CallFailedView,
    _getContactDisplayName: _getContactDisplayName,
    GenericFailureView: GenericFailureView,
    IncomingCallView: IncomingCallView,
    IncomingConversationView: IncomingConversationView,
    OngoingConversationView: OngoingConversationView,
    OutgoingConversationView: OutgoingConversationView
  };

})(document.mozL10n || navigator.mozL10n);
