/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedViews = loop.shared.views;

  /**
   * ActiveRoomStore mixin.
   * @type {Object}
   */
  var ActiveRoomStoreMixin = {
    mixins: [Backbone.Events],

    propTypes: {
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    componentWillMount: function() {
      this.listenTo(this.props.roomStore, "change:activeRoom",
                    this._onActiveRoomStateChanged);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.roomStore);
    },

    _onActiveRoomStateChanged: function() {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState(this.props.roomStore.getStoreState("activeRoom"));
      }
    },

    getInitialState: function() {
      var storeState = this.props.roomStore.getStoreState("activeRoom");
      return _.extend({
        // Used by the UI showcase.
        roomState: this.props.roomState || storeState.roomState
      }, storeState);
    }
  };

  /**
   * Desktop room invitation view (overlay).
   */
  var DesktopRoomInvitationView = React.createClass({
    mixins: [ActiveRoomStoreMixin, React.addons.LinkedStateMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    getInitialState: function() {
      return {
        copiedUrl: false,
        newRoomName: "",
        error: null,
      };
    },

    componentWillMount: function() {
      this.listenTo(this.props.roomStore, "change:error",
                    this.onRoomError);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.roomStore);
    },

    handleTextareaKeyDown: function(event) {
      // Submit the form as soon as the user press Enter in that field
      // Note: We're using a textarea instead of a simple text input to display
      // placeholder and entered text on two lines, to circumvent l10n
      // rendering/UX issues for some locales.
      if (event.which === 13) {
        this.handleFormSubmit(event);
      }
    },

    handleFormSubmit: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.RenameRoom({
        roomToken: this.state.roomToken,
        newRoomName: this.state.newRoomName
      }));
    },

    handleEmailButtonClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(
        new sharedActions.EmailRoomUrl({roomUrl: this.state.roomUrl}));
    },

    handleCopyButtonClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(
        new sharedActions.CopyRoomUrl({roomUrl: this.state.roomUrl}));

      this.setState({copiedUrl: true});
    },

    onRoomError: function() {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState({error: this.props.roomStore.getStoreState("error")});
      }
    },

    render: function() {
      var cx = React.addons.classSet;
      return (
        <div className="room-invitation-overlay">
          <p className={cx({"error": !!this.state.error,
                            "error-display-area": true})}>
            {mozL10n.get("rooms_name_change_failed_label")}
          </p>
          <form onSubmit={this.handleFormSubmit}>
            <textarea rows="2" type="text" className="input-room-name"
              valueLink={this.linkState("newRoomName")}
              onBlur={this.handleFormSubmit}
              onKeyDown={this.handleTextareaKeyDown}
              placeholder={mozL10n.get("rooms_name_this_room_label")} />
          </form>
          <p>{mozL10n.get("invite_header_text")}</p>
          <div className="btn-group call-action-group">
            <button className="btn btn-info btn-email"
                    onClick={this.handleEmailButtonClick}>
              {mozL10n.get("share_button2")}
            </button>
            <button className="btn btn-info btn-copy"
                    onClick={this.handleCopyButtonClick}>
              {this.state.copiedUrl ? mozL10n.get("copied_url_button") :
                                      mozL10n.get("copy_url_button2")}
            </button>
          </div>
        </div>
      );
    }
  });

  /**
   * Desktop room conversation view.
   */
  var DesktopRoomConversationView = React.createClass({
    mixins: [
      ActiveRoomStoreMixin,
      sharedMixins.DocumentTitleMixin,
      sharedMixins.MediaSetupMixin,
      sharedMixins.RoomsAudioMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    _renderInvitationOverlay: function() {
      if (this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS) {
        return <DesktopRoomInvitationView
          roomStore={this.props.roomStore}
          dispatcher={this.props.dispatcher}
        />;
      }
      return null;
    },

    componentWillUpdate: function(nextProps, nextState) {
      // The SDK needs to know about the configuration and the elements to use
      // for display. So the best way seems to pass the information here - ideally
      // the sdk wouldn't need to know this, but we can't change that.
      if (this.state.roomState !== ROOM_STATES.MEDIA_WAIT &&
          nextState.roomState === ROOM_STATES.MEDIA_WAIT) {
        this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
          publisherConfig: this.getDefaultPublisherConfig({
            publishVideo: !this.state.videoMuted
          }),
          getLocalElementFunc: this._getElement.bind(this, ".local"),
          getRemoteElementFunc: this._getElement.bind(this, ".remote")
        }));
      }
    },

    /**
     * User clicked on the "Leave" button.
     */
    leaveRoom: function() {
      this.props.dispatcher.dispatch(new sharedActions.LeaveRoom());
    },

    /**
     * Closes the window if the cancel button is pressed in the generic failure view.
     */
    closeWindow: function() {
      window.close();
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
      if (this.state.roomName) {
        this.setTitle(this.state.roomName);
      }

      var localStreamClasses = React.addons.classSet({
        local: true,
        "local-stream": true,
        "local-stream-audio": this.state.videoMuted,
        "room-preview": this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS
      });

      switch(this.state.roomState) {
        case ROOM_STATES.FAILED:
        case ROOM_STATES.FULL: {
          // Note: While rooms are set to hold a maximum of 2 participants, the
          //       FULL case should never happen on desktop.
          return <loop.conversationViews.GenericFailureView
            cancelCall={this.closeWindow}
          />;
        }
        case ROOM_STATES.ENDED: {
          if (this.state.used)
            return <sharedViews.FeedbackView
              onAfterFeedbackReceived={this.closeWindow}
            />;

          // In case the room was not used (no one was here), we
          // bypass the feedback form.
          this.closeWindow();
          return null;
        }
        default: {
          return (
            <div className="room-conversation-wrapper">
              {this._renderInvitationOverlay()}
              <div className="video-layout-wrapper">
                <div className="conversation room-conversation">
                  <div className="media nested">
                    <div className="video_wrapper remote_wrapper">
                      <div className="video_inner remote"></div>
                    </div>
                    <div className={localStreamClasses}></div>
                  </div>
                  <sharedViews.ConversationToolbar
                    video={{enabled: !this.state.videoMuted, visible: true}}
                    audio={{enabled: !this.state.audioMuted, visible: true}}
                    publishStream={this.publishStream}
                    hangup={this.leaveRoom} />
                </div>
              </div>
            </div>
          );
        }
      }
    }
  });

  return {
    ActiveRoomStoreMixin: ActiveRoomStoreMixin,
    DesktopRoomConversationView: DesktopRoomConversationView,
    DesktopRoomInvitationView: DesktopRoomInvitationView
  };

})(document.mozL10n || navigator.mozL10n);
