/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.conversationViews = (function(mozL10n) {

  var CALL_STATES = loop.store.CALL_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
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
    mixins: [sharedMixins.DropdownMenuMixin],

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
        showMenu: false,
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
      if (!target.classList.contains('btn-chevron')) {
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
      return false;
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
      /* jshint ignore:start */
      var dropdownMenuClassesDecline = React.addons.classSet({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "visually-hidden": !this.state.showMenu
      });

      return (
        React.createElement("div", {className: "call-window"}, 
          React.createElement(CallIdentifierView, {video: this.props.callType === CALL_TYPES.AUDIO_VIDEO, 
            peerIdentifier: this.props.callerId, 
            showIcons: true}), 

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
      /* jshint ignore:end */
    }
  });

  /**
   * Incoming call view accept button, renders different primary actions
   * (answer with video / with audio only) based on the props received
   **/
  var AcceptCallButton = React.createClass({displayName: "AcceptCallButton",

    propTypes: {
      mode: React.PropTypes.object.isRequired,
    },

    render: function() {
      var mode = this.props.mode;
      return (
        /* jshint ignore:start */
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
        /* jshint ignore:end */
      );
    }
  });

  /**
   * Something went wrong view. Displayed when there's a big problem.
   */
  var GenericFailureView = React.createClass({displayName: "GenericFailureView",
    mixins: [
      sharedMixins.AudioMixin,
      sharedMixins.DocumentTitleMixin
    ],

    propTypes: {
      cancelCall: React.PropTypes.func.isRequired,
      failureReason: React.PropTypes.string
    },

    componentDidMount: function() {
      this.play("failure");
    },

    render: function() {
      this.setTitle(mozL10n.get("generic_failure_title"));

      var errorString;
      switch (this.props.failureReason) {
        case FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA:
          errorString = mozL10n.get("no_media_failure_message");
          break;
        default:
          errorString = mozL10n.get("generic_failure_title");
      }

      return (
        React.createElement("div", {className: "call-window"}, 
          React.createElement("h2", null, errorString), 

          React.createElement("div", {className: "btn-group call-action-group"}, 
            React.createElement("button", {className: "btn btn-cancel", 
                    onClick: this.props.cancelCall}, 
              mozL10n.get("cancel_button")
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
   * Call failed view. Displayed when a call fails.
   */
  var CallFailedView = React.createClass({displayName: "CallFailedView",
    mixins: [
      Backbone.Events,
      loop.store.StoreMixin("conversationStore"),
      sharedMixins.AudioMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      contact: React.PropTypes.object.isRequired,
      // This is used by the UI showcase.
      emailLinkError: React.PropTypes.bool,
      outgoing: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      return {
        emailLinkError: this.props.emailLinkError,
        emailLinkButtonDisabled: false
      };
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
      return React.createElement("p", {className: "error"}, mozL10n.get("unable_retrieve_url"));
    },

    _getTitleMessage: function() {
      switch (this.getStoreState().callStateReason) {
        case WEBSOCKET_REASONS.REJECT:
        case WEBSOCKET_REASONS.BUSY:
        case REST_ERRNOS.USER_UNAVAILABLE:
          var contactDisplayName = _getContactDisplayName(this.props.contact);
          if (contactDisplayName.length) {
            return mozL10n.get(
              "contact_unavailable_title",
              {"contactName": contactDisplayName});
          }

          return mozL10n.get("generic_contact_unavailable_title");
        case FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA:
          return mozL10n.get("no_media_failure_message");
        default:
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

    _renderMessage: function() {
      if (this.props.outgoing) {
        return  (React.createElement("p", {className: "btn-label"}, mozL10n.get("generic_failure_with_reason2")));
      }

      return null;
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

      return (
        React.createElement("div", {className: "call-window"}, 
          React.createElement("h2", null,  this._getTitleMessage() ), 

          this._renderMessage(), 
          this._renderError(), 

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
                    onClick: this.emailLink, 
                    disabled: this.state.emailLinkButtonDisabled}, 
              mozL10n.get("share_button3")
            )
          )
        )
      );
    }
  });

  var OngoingConversationView = React.createClass({displayName: "OngoingConversationView",
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
        React.createElement("div", {className: "video-layout-wrapper"}, 
          React.createElement("div", {className: "conversation"}, 
            React.createElement("div", {className: "media nested"}, 
              React.createElement("div", {className: "video_wrapper remote_wrapper"}, 
                React.createElement("div", {className: "video_inner remote focus-stream"})
              ), 
              React.createElement("div", {className: localStreamClasses})
            ), 
            React.createElement(loop.shared.views.ConversationToolbar, {
              video: this.props.video, 
              audio: this.props.audio, 
              publishStream: this.publishStream, 
              hangup: this.hangup})
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
      mozLoop: React.PropTypes.object.isRequired
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

    /**
     * Used to setup and render the feedback view.
     */
    _renderFeedbackView: function() {
      this.setTitle(mozL10n.get("conversation_has_ended"));

      return (
        React.createElement(sharedViews.FeedbackView, {
          onAfterFeedbackReceived: this._closeWindow.bind(this)}
        )
      );
    },

    _renderViewFromCallType: function() {
      // For outgoing calls we can display the pending conversation view
      // for any state that render() doesn't manage.
      if (this.state.outgoing) {
        return (React.createElement(PendingConversationView, {
          dispatcher: this.props.dispatcher, 
          callState: this.state.callState, 
          contact: this.state.contact, 
          enableCancelButton: this._isCancellable()}
        ));
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
          return (React.createElement(CallFailedView, {
            dispatcher: this.props.dispatcher, 
            contact: this.state.contact, 
            outgoing: this.state.outgoing}
          ));
        }
        case CALL_STATES.ONGOING: {
          return (React.createElement(OngoingConversationView, {
            dispatcher: this.props.dispatcher, 
            video: {enabled: !this.state.videoMuted}, 
            audio: {enabled: !this.state.audioMuted}}
            )
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
          return this._renderViewFromCallType();
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
    AcceptCallView: AcceptCallView,
    OngoingConversationView: OngoingConversationView,
    CallControllerView: CallControllerView
  };

})(document.mozL10n || navigator.mozL10n);
