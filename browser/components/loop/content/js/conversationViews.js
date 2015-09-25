/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.conversationViews = (function(mozL10n) {
  "use strict";

  var CALL_STATES = loop.store.CALL_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sharedViews = loop.shared.views;
  var sharedMixins = loop.shared.mixins;

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
    return contact.email.find(function find(e) { return e.pref; }) || contact.email[0];
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
  var CallIdentifierView = React.createClass({displayName: "CallIdentifierView",
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
        React.createElement("div", {className: "fx-embedded-call-identifier"}, 
          React.createElement("div", {className: "fx-embedded-call-identifier-avatar fx-embedded-call-identifier-item"}), 
          React.createElement("div", {className: "fx-embedded-call-identifier-info fx-embedded-call-identifier-item"}, 
            React.createElement("div", {className: "fx-embedded-call-identifier-text overflow-text-ellipsis"}, 
              this.props.peerIdentifier
            ), 
            React.createElement("div", {className: callDetailClasses}, 
              React.createElement("span", {className: "fx-embedded-tiny-audio-icon"}), 
              React.createElement("span", {className: iconVideoClasses}), 
              React.createElement("span", {className: "fx-embedded-conversation-timestamp"}, 
                this.formatCreationDate()
              )
            )
          )
        )
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
  var ConversationDetailView = React.createClass({displayName: "ConversationDetailView",
    propTypes: {
      children: React.PropTypes.oneOfType([
        React.PropTypes.element,
        React.PropTypes.arrayOf(React.PropTypes.element)
      ]).isRequired,
      contact: React.PropTypes.object
    },

    render: function() {
      var contactName = _getContactDisplayName(this.props.contact);

      return (
        React.createElement("div", {className: "call-window"}, 
          React.createElement(CallIdentifierView, {
            peerIdentifier: contactName, 
            showIcons: false}), 
          React.createElement("div", null, this.props.children)
        )
      );
    }
  });

  // Matches strings of the form "<nonspaces>@<nonspaces>" or "+<digits>"
  var EMAIL_OR_PHONE_RE = /^(:?\S+@\S+|\+\d+)$/;

  var AcceptCallView = React.createClass({displayName: "AcceptCallView",
    mixins: [sharedMixins.DropdownMenuMixin()],

    propTypes: {
      callType: React.PropTypes.string.isRequired,
      callerId: React.PropTypes.string.isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      // Only for use by the ui-showcase
      showMenu: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {
        showMenu: false
      };
    },

    componentDidMount: function() {
      this.props.mozLoop.startAlerting();
    },

    componentWillUnmount: function() {
      this.props.mozLoop.stopAlerting();
    },

    clickHandler: function(e) {
      var target = e.target;
      if (!target.classList.contains("btn-chevron")) {
        this._hideDeclineMenu();
      }
    },

    _handleAccept: function(callType) {
      return function() {
        this.props.dispatcher.dispatch(new sharedActions.AcceptCall({
          callType: callType
        }));
      }.bind(this);
    },

    _handleDecline: function() {
      this.props.dispatcher.dispatch(new sharedActions.DeclineCall({
        blockCaller: false
      }));
    },

    _handleDeclineBlock: function(e) {
      this.props.dispatcher.dispatch(new sharedActions.DeclineCall({
        blockCaller: true
      }));

      /* Prevent event propagation
       * stop the click from reaching parent element */
       e.stopPropagation();
       e.preventDefault();
    },

    /*
     * Generate props for <AcceptCallButton> component based on
     * incoming call type. An incoming video call will render a video
     * answer button primarily, an audio call will flip them.
     **/
    _answerModeProps: function() {
      var videoButton = {
        handler: this._handleAccept(CALL_TYPES.AUDIO_VIDEO),
        className: "fx-embedded-btn-icon-video",
        tooltip: "incoming_call_accept_audio_video_tooltip"
      };
      var audioButton = {
        handler: this._handleAccept(CALL_TYPES.AUDIO_ONLY),
        className: "fx-embedded-btn-audio-small",
        tooltip: "incoming_call_accept_audio_only_tooltip"
      };
      var props = {};
      props.primary = videoButton;
      props.secondary = audioButton;

      // When video is not enabled on this call, we swap the buttons around.
      if (this.props.callType === CALL_TYPES.AUDIO_ONLY) {
        audioButton.className = "fx-embedded-btn-icon-audio";
        videoButton.className = "fx-embedded-btn-video-small";
        props.primary = audioButton;
        props.secondary = videoButton;
      }

      return props;
    },

    render: function() {
      var dropdownMenuClassesDecline = React.addons.classSet({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "visually-hidden": !this.state.showMenu
      });

      return (
        React.createElement("div", {className: "call-window"}, 
          React.createElement(CallIdentifierView, {
            peerIdentifier: this.props.callerId, 
            showIcons: true, 
            video: this.props.callType === CALL_TYPES.AUDIO_VIDEO}), 

          React.createElement("div", {className: "btn-group call-action-group"}, 

            React.createElement("div", {className: "fx-embedded-call-button-spacer"}), 

            React.createElement("div", {className: "btn-chevron-menu-group"}, 
              React.createElement("div", {className: "btn-group-chevron"}, 
                React.createElement("div", {className: "btn-group"}, 

                  React.createElement("button", {className: "btn btn-decline", 
                          onClick: this._handleDecline}, 
                    mozL10n.get("incoming_call_cancel_button")
                  ), 
                  React.createElement("div", {className: "btn-chevron", 
                       onClick: this.toggleDropdownMenu, 
                       ref: "menu-button"})
                ), 

                React.createElement("ul", {className: dropdownMenuClassesDecline}, 
                  React.createElement("li", {className: "btn-block", onClick: this._handleDeclineBlock}, 
                    mozL10n.get("incoming_call_cancel_and_block_button")
                  )
                )

              )
            ), 

            React.createElement("div", {className: "fx-embedded-call-button-spacer"}), 

            React.createElement(AcceptCallButton, {mode: this._answerModeProps()}), 

            React.createElement("div", {className: "fx-embedded-call-button-spacer"})

          )
        )
      );
    }
  });

  /**
   * Incoming call view accept button, renders different primary actions
   * (answer with video / with audio only) based on the props received
   **/
  var AcceptCallButton = React.createClass({displayName: "AcceptCallButton",

    propTypes: {
      mode: React.PropTypes.object.isRequired
    },

    render: function() {
      var mode = this.props.mode;
      return (
        React.createElement("div", {className: "btn-chevron-menu-group"}, 
          React.createElement("div", {className: "btn-group"}, 
            React.createElement("button", {className: "btn btn-accept", 
                    onClick: mode.primary.handler, 
                    title: mozL10n.get(mode.primary.tooltip)}, 
              React.createElement("span", {className: "fx-embedded-answer-btn-text"}, 
                mozL10n.get("incoming_call_accept_button")
              ), 
              React.createElement("span", {className: mode.primary.className})
            ), 
            React.createElement("div", {className: mode.secondary.className, 
                 onClick: mode.secondary.handler, 
                 title: mozL10n.get(mode.secondary.tooltip)}
            )
          )
        )
      );
    }
  });

  /**
   * View for pending conversations. Displays a cancel button and appropriate
   * pending/ringing strings.
   */
  var PendingConversationView = React.createClass({displayName: "PendingConversationView",
    mixins: [sharedMixins.AudioMixin],

    propTypes: {
      callState: React.PropTypes.string,
      contact: React.PropTypes.object,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
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
        React.createElement(ConversationDetailView, {contact: this.props.contact}, 

          React.createElement("p", {className: "btn-label"}, pendingStateString), 

          React.createElement("div", {className: "btn-group call-action-group"}, 
            React.createElement("button", {className: btnCancelStyles, 
                    onClick: this.cancelCall}, 
              mozL10n.get("initiate_call_cancel_button")
            )
          )

        )
      );
    }
  });

  /**
   * Used to display errors in direct calls and rooms to the user.
   */
  var FailureInfoView = React.createClass({displayName: "FailureInfoView",
    propTypes: {
      contact: React.PropTypes.object,
      extraFailureMessage: React.PropTypes.string,
      extraMessage: React.PropTypes.string,
      failureReason: React.PropTypes.string.isRequired
    },

    /**
     * Returns the translated message appropraite to the failure reason.
     *
     * @return {String} The translated message for the failure reason.
     */
    _getMessage: function() {
      switch (this.props.failureReason) {
        case FAILURE_DETAILS.USER_UNAVAILABLE:
          var contactDisplayName = _getContactDisplayName(this.props.contact);
          if (contactDisplayName.length) {
            return mozL10n.get(
              "contact_unavailable_title",
              {"contactName": contactDisplayName});
          }
          return mozL10n.get("generic_contact_unavailable_title");
        case FAILURE_DETAILS.NO_MEDIA:
        case FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA:
          return mozL10n.get("no_media_failure_message");
        case FAILURE_DETAILS.TOS_FAILURE:
          return mozL10n.get("tos_failure_message",
            { clientShortname: mozL10n.get("clientShortname2") });
        default:
          return mozL10n.get("generic_failure_message");
      }
    },

    _renderExtraMessage: function() {
      if (this.props.extraMessage) {
        return React.createElement("p", {className: "failure-info-extra"}, this.props.extraMessage);
      }
      return null;
    },

    _renderExtraFailureMessage: function() {
      if (this.props.extraFailureMessage) {
        return React.createElement("p", {className: "failure-info-extra-failure"}, this.props.extraFailureMessage);
      }
      return null;
    },

    render: function() {
      return (
        React.createElement("div", {className: "failure-info"}, 
          React.createElement("div", {className: "failure-info-logo"}), 
          React.createElement("h2", {className: "failure-info-message"}, this._getMessage()), 
          this._renderExtraMessage(), 
          this._renderExtraFailureMessage()
        )
      );
    }
  });

  /**
   * Direct Call failure view. Displayed when a call fails.
   */
  var DirectCallFailureView = React.createClass({displayName: "DirectCallFailureView",
    mixins: [
      Backbone.Events,
      loop.store.StoreMixin("conversationStore"),
      sharedMixins.AudioMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      // This is used by the UI showcase.
      emailLinkError: React.PropTypes.bool,
      mozLoop: React.PropTypes.object.isRequired,
      outgoing: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      return _.extend({
        emailLinkError: this.props.emailLinkError,
        emailLinkButtonDisabled: false
      }, this.getStoreState());
    },

    componentDidMount: function() {
      this.play("failure");
      this.listenTo(this.getStore(), "change:emailLink",
                    this._onEmailLinkReceived);
      this.listenTo(this.getStore(), "error:emailLink",
                    this._onEmailLinkError);
    },

    componentWillUnmount: function() {
      this.stopListening(this.getStore());
    },

    _onEmailLinkReceived: function() {
      var emailLink = this.getStoreState().emailLink;
      var contactEmail = _getPreferredEmail(this.state.contact).value;
      sharedUtils.composeCallUrlEmail(emailLink, contactEmail, null, "callfailed");
      this.closeWindow();
    },

    _onEmailLinkError: function() {
      this.setState({
        emailLinkError: true,
        emailLinkButtonDisabled: false
      });
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
        roomName: _getContactDisplayName(this.state.contact)
      }));
    },

    render: function() {
      var cx = React.addons.classSet;

      var retryClasses = cx({
        btn: true,
        "btn-info": true,
        "btn-retry": true,
        hide: !this.props.outgoing
      });
      var emailClasses = cx({
        btn: true,
        "btn-info": true,
        "btn-email": true,
        hide: !this.props.outgoing
      });

      var settingsMenuItems = [
        { id: "feedback" },
        { id: "help" }
      ];

      var extraMessage;

      if (this.props.outgoing) {
        extraMessage = mozL10n.get("generic_failure_with_reason2");
      }

      var extraFailureMessage;

      if (this.state.emailLinkError) {
        extraFailureMessage = mozL10n.get("unable_retrieve_url");
      }

      return (
        React.createElement("div", {className: "direct-call-failure"}, 
          React.createElement(FailureInfoView, {
            contact: this.state.contact, 
            extraFailureMessage: extraFailureMessage, 
            extraMessage: extraMessage, 
            failureReason: this.getStoreState().callStateReason}), 

          React.createElement("div", {className: "btn-group call-action-group"}, 
            React.createElement("button", {className: "btn btn-cancel", 
                    onClick: this.cancelCall}, 
              mozL10n.get("cancel_button")
            ), 
            React.createElement("button", {className: retryClasses, 
                    onClick: this.retryCall}, 
              mozL10n.get("retry_call_button")
            ), 
            React.createElement("button", {className: emailClasses, 
                    disabled: this.state.emailLinkButtonDisabled, 
                    onClick: this.emailLink}, 
              mozL10n.get("share_button3")
            )
          ), 
          React.createElement(loop.shared.views.SettingsControlButton, {
            menuBelow: true, 
            menuItems: settingsMenuItems, 
            mozLoop: this.props.mozLoop})
        )
      );
    }
  });

  var OngoingConversationView = React.createClass({displayName: "OngoingConversationView",
    mixins: [
      sharedMixins.MediaSetupMixin
    ],

    propTypes: {
      // local
      audio: React.PropTypes.object,
      // We pass conversationStore here rather than use the mixin, to allow
      // easy configurability for the ui-showcase.
      conversationStore: React.PropTypes.instanceOf(loop.store.ConversationStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      // The poster URLs are for UI-showcase testing and development.
      localPosterUrl: React.PropTypes.string,
      // This is used from the props rather than the state to make it easier for
      // the ui-showcase.
      mediaConnected: React.PropTypes.bool,
      mozLoop: React.PropTypes.object,
      remotePosterUrl: React.PropTypes.string,
      remoteVideoEnabled: React.PropTypes.bool,
      // local
      video: React.PropTypes.object
    },

    getDefaultProps: function() {
      return {
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true}
      };
    },

    getInitialState: function() {
      return this.props.conversationStore.getStoreState();
    },

    componentWillMount: function() {
      this.props.conversationStore.on("change", function() {
        this.setState(this.props.conversationStore.getStoreState());
      }, this);
    },

    componentWillUnmount: function() {
      this.props.conversationStore.off("change", null, this);
    },

    componentDidMount: function() {
      // The SDK needs to know about the configuration and the elements to use
      // for display. So the best way seems to pass the information here - ideally
      // the sdk wouldn't need to know this, but we can't change that.
      this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
        publisherConfig: this.getDefaultPublisherConfig({
          publishVideo: this.props.video.enabled
        })
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

    /**
     * Should we render a visual cue to the user (e.g. a spinner) that a local
     * stream is on its way from the camera?
     *
     * @returns {boolean}
     * @private
     */
    _isLocalLoading: function () {
      return !this.state.localSrcMediaElement && !this.props.localPosterUrl;
    },

    /**
     * Should we render a visual cue to the user (e.g. a spinner) that a remote
     * stream is on its way from the other user?
     *
     * @returns {boolean}
     * @private
     */
    _isRemoteLoading: function() {
      return !!(!this.state.remoteSrcMediaElement &&
                !this.props.remotePosterUrl &&
                !this.state.mediaConnected);
    },

    shouldRenderRemoteVideo: function() {
      if (this.props.mediaConnected) {
        // If remote video is not enabled, we're muted, so we'll show an avatar
        // instead.
        return this.props.remoteVideoEnabled;
      }

      // We're not yet connected, but we don't want to show the avatar, and in
      // the common case, we'll just transition to the video.
      return true;
    },

    render: function() {
      // 'visible' and 'enabled' are true by default.
      var settingsMenuItems = [
        {
          id: "edit",
          visible: false,
          enabled: false
        },
        { id: "feedback" },
        { id: "help" }
      ];
      return (
        React.createElement("div", {className: "desktop-call-wrapper"}, 
          React.createElement(sharedViews.MediaLayoutView, {
            dispatcher: this.props.dispatcher, 
            displayScreenShare: false, 
            isLocalLoading: this._isLocalLoading(), 
            isRemoteLoading: this._isRemoteLoading(), 
            isScreenShareLoading: false, 
            localPosterUrl: this.props.localPosterUrl, 
            localSrcMediaElement: this.state.localSrcMediaElement, 
            localVideoMuted: !this.props.video.enabled, 
            matchMedia: this.state.matchMedia || window.matchMedia.bind(window), 
            remotePosterUrl: this.props.remotePosterUrl, 
            remoteSrcMediaElement: this.state.remoteSrcMediaElement, 
            renderRemoteVideo: this.shouldRenderRemoteVideo(), 
            screenShareMediaElement: this.state.screenShareMediaElement, 
            screenSharePosterUrl: null, 
            showContextRoomName: false, 
            useDesktopPaths: true}, 
            React.createElement(loop.shared.views.ConversationToolbar, {
              audio: this.props.audio, 
              dispatcher: this.props.dispatcher, 
              hangup: this.hangup, 
              mozLoop: this.props.mozLoop, 
              publishStream: this.publishStream, 
              settingsMenuItems: settingsMenuItems, 
              show: true, 
              video: this.props.video})
          )
        )
      );
    }
  });

  /**
   * Master View Controller for outgoing calls. This manages
   * the different views that need displaying.
   */
  var CallControllerView = React.createClass({displayName: "CallControllerView",
    mixins: [
      sharedMixins.AudioMixin,
      sharedMixins.DocumentTitleMixin,
      loop.store.StoreMixin("conversationStore"),
      Backbone.Events
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      onCallTerminated: React.PropTypes.func.isRequired
    },

    getInitialState: function() {
      return this.getStoreState();
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

    _renderViewFromCallType: function() {
      // For outgoing calls we can display the pending conversation view
      // for any state that render() doesn't manage.
      if (this.state.outgoing) {
        return (React.createElement(PendingConversationView, {
          callState: this.state.callState, 
          contact: this.state.contact, 
          dispatcher: this.props.dispatcher, 
          enableCancelButton: this._isCancellable()}));
      }

      // For incoming calls that are in accepting state, display the
      // accept call view.
      if (this.state.callState === CALL_STATES.ALERTING) {
        return (React.createElement(AcceptCallView, {
          callType: this.state.callType, 
          callerId: this.state.callerId, 
          dispatcher: this.props.dispatcher, 
          mozLoop: this.props.mozLoop}
        ));
      }

      // Otherwise we're still gathering or connecting, so
      // don't display anything.
      return null;
    },

    componentDidUpdate: function(prevProps, prevState) {
      // Handle timestamp and window closing only when the call has terminated.
      if (prevState.callState === CALL_STATES.ONGOING &&
          this.state.callState === CALL_STATES.FINISHED) {
        this.props.onCallTerminated();
      }
    },

    render: function() {
      // Set the default title to the contact name or the callerId, note
      // that views may override this, e.g. the feedback view.
      if (this.state.contact) {
        this.setTitle(_getContactDisplayName(this.state.contact));
      } else {
        this.setTitle(this.state.callerId || "");
      }

      switch (this.state.callState) {
        case CALL_STATES.CLOSE: {
          this._closeWindow();
          return null;
        }
        case CALL_STATES.TERMINATED: {
          return (React.createElement(DirectCallFailureView, {
            dispatcher: this.props.dispatcher, 
            mozLoop: this.props.mozLoop, 
            outgoing: this.state.outgoing}));
        }
        case CALL_STATES.ONGOING: {
          return (React.createElement(OngoingConversationView, {
            audio: { enabled: !this.state.audioMuted, visible: true}, 
            conversationStore: this.getStore(), 
            dispatcher: this.props.dispatcher, 
            mediaConnected: this.state.mediaConnected, 
            mozLoop: this.props.mozLoop, 
            remoteSrcMediaElement: this.state.remoteSrcMediaElement, 
            remoteVideoEnabled: this.state.remoteVideoEnabled, 
            video: { enabled: !this.state.videoMuted, visible: true}})
          );
        }
        case CALL_STATES.FINISHED: {
          this.play("terminated");

          // When conversation ended we either display a feedback form or
          // close the window. This is decided in the AppControllerView.
          return null;
        }
        case CALL_STATES.INIT: {
          // We know what we are, but we haven't got the data yet.
          return null;
        }
        default: {
          return this._renderViewFromCallType();
        }
      }
    }
  });

  return {
    PendingConversationView: PendingConversationView,
    CallIdentifierView: CallIdentifierView,
    ConversationDetailView: ConversationDetailView,
    _getContactDisplayName: _getContactDisplayName,
    FailureInfoView: FailureInfoView,
    DirectCallFailureView: DirectCallFailureView,
    AcceptCallView: AcceptCallView,
    OngoingConversationView: OngoingConversationView,
    CallControllerView: CallControllerView
  };

})(document.mozL10n || navigator.mozL10n);
