/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      children: React.PropTypes.oneOfType([
        React.PropTypes.element,
        React.PropTypes.arrayOf(React.PropTypes.element)
      ]).isRequired,
      contact: React.PropTypes.object
    },

    render: function() {
      var contactName = _getContactDisplayName(this.props.contact);

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

  var AcceptCallView = React.createClass({
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
      var dropdownMenuClassesDecline = React.addons.classSet({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "visually-hidden": !this.state.showMenu
      });

      return (
        <div className="call-window">
          <CallIdentifierView
            peerIdentifier={this.props.callerId}
            showIcons={true}
            video={this.props.callType === CALL_TYPES.AUDIO_VIDEO} />

          <div className="btn-group call-action-group">

            <div className="fx-embedded-call-button-spacer"></div>

            <div className="btn-chevron-menu-group">
              <div className="btn-group-chevron">
                <div className="btn-group">

                  <button className="btn btn-decline"
                          onClick={this._handleDecline}>
                    {mozL10n.get("incoming_call_cancel_button")}
                  </button>
                  <div className="btn-chevron"
                       onClick={this.toggleDropdownMenu}
                       ref="menu-button" />
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
    }
  });

  /**
   * Incoming call view accept button, renders different primary actions
   * (answer with video / with audio only) based on the props received
   **/
  var AcceptCallButton = React.createClass({

    propTypes: {
      mode: React.PropTypes.object.isRequired
    },

    render: function() {
      var mode = this.props.mode;
      return (
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
      );
    }
  });

  /**
   * Something went wrong view. Displayed when there's a big problem.
   */
  var GenericFailureView = React.createClass({
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
        case FAILURE_DETAILS.NO_MEDIA:
        case FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA:
          errorString = mozL10n.get("no_media_failure_message");
          break;
        default:
          errorString = mozL10n.get("generic_failure_title");
      }

      return (
        <div className="call-window">
          <h2>{errorString}</h2>

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
   * View for pending conversations. Displays a cancel button and appropriate
   * pending/ringing strings.
   */
  var PendingConversationView = React.createClass({
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
      loop.store.StoreMixin("conversationStore"),
      sharedMixins.AudioMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      contact: React.PropTypes.object.isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
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
      sharedUtils.composeCallUrlEmail(emailLink, contactEmail, null, "callfailed");
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
        return  (<p className="btn-label">{mozL10n.get("generic_failure_with_reason2")}</p>);
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
        <div className="call-window">
          <h2>{ this._getTitleMessage() }</h2>

          {this._renderMessage()}
          {this._renderError()}

          <div className="btn-group call-action-group">
            <button className="btn btn-cancel"
                    onClick={this.cancelCall}>
              {mozL10n.get("cancel_button")}
            </button>
            <button className={retryClasses}
                    onClick={this.retryCall}>
              {mozL10n.get("retry_call_button")}
            </button>
            <button className={emailClasses}
                    disabled={this.state.emailLinkButtonDisabled}
                    onClick={this.emailLink}>
              {mozL10n.get("share_button3")}
            </button>
          </div>
        </div>
      );
    }
  });

  var OngoingConversationView = React.createClass({
    mixins: [
      loop.store.StoreMixin("conversationStore"),
      sharedMixins.MediaSetupMixin
    ],

    propTypes: {
      // local
      audio: React.PropTypes.object,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      // The poster URLs are for UI-showcase testing and development.
      localPosterUrl: React.PropTypes.string,
      // This is used from the props rather than the state to make it easier for
      // the ui-showcase.
      mediaConnected: React.PropTypes.bool,
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
      return this.getStoreState();
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
                <div className="video_inner remote focus-stream">
                  <sharedViews.MediaView displayAvatar={!this.shouldRenderRemoteVideo()}
                    isLoading={false}
                    mediaType="remote"
                    posterUrl={this.props.remotePosterUrl}
                    srcVideoObject={this.state.remoteSrcVideoObject} />
                </div>
              </div>
              <div className={localStreamClasses}>
                <sharedViews.MediaView displayAvatar={!this.props.video.enabled}
                  isLoading={false}
                  mediaType="local"
                  posterUrl={this.props.localPosterUrl}
                  srcVideoObject={this.state.localSrcVideoObject} />
              </div>
            </div>
            <loop.shared.views.ConversationToolbar
              audio={this.props.audio}
              dispatcher={this.props.dispatcher}
              edit={{ visible: false, enabled: false }}
              hangup={this.hangup}
              publishStream={this.publishStream}
              video={this.props.video} />
          </div>
        </div>
      );
    }
  });

  /**
   * Master View Controller for outgoing calls. This manages
   * the different views that need displaying.
   */
  var CallControllerView = React.createClass({
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
        <sharedViews.FeedbackView
          onAfterFeedbackReceived={this._closeWindow.bind(this)}
        />
      );
    },

    _renderViewFromCallType: function() {
      // For outgoing calls we can display the pending conversation view
      // for any state that render() doesn't manage.
      if (this.state.outgoing) {
        return (<PendingConversationView
          callState={this.state.callState}
          contact={this.state.contact}
          dispatcher={this.props.dispatcher}
          enableCancelButton={this._isCancellable()} />);
      }

      // For incoming calls that are in accepting state, display the
      // accept call view.
      if (this.state.callState === CALL_STATES.ALERTING) {
        return (<AcceptCallView
          callType={this.state.callType}
          callerId={this.state.callerId}
          dispatcher={this.props.dispatcher}
          mozLoop={this.props.mozLoop}
        />);
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
          return (<CallFailedView
            contact={this.state.contact}
            dispatcher={this.props.dispatcher}
            outgoing={this.state.outgoing} />);
        }
        case CALL_STATES.ONGOING: {
          return (<OngoingConversationView
            audio={{enabled: !this.state.audioMuted}}
            dispatcher={this.props.dispatcher}
            mediaConnected={this.state.mediaConnected}
            remoteSrcVideoObject={this.state.remoteSrcVideoObject}
            remoteVideoEnabled={this.state.remoteVideoEnabled}
            video={{enabled: !this.state.videoMuted}} />
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
    }
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
