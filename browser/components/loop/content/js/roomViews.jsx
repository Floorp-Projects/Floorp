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

  function noop() {}

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
        newRoomName: ""
      };
    },

    handleFormSubmit: function(event) {
      event.preventDefault();

      var newRoomName = this.state.newRoomName;

      if (newRoomName && this.state.roomName != newRoomName) {
        this.props.dispatcher.dispatch(
          new sharedActions.RenameRoom({
            roomToken: this.state.roomToken,
            newRoomName: newRoomName
          }));
      }
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

    render: function() {
      return (
        <div className="room-invitation-overlay">
          <form onSubmit={this.handleFormSubmit}>
            <input type="text" className="input-room-name"
              valueLink={this.linkState("newRoomName")}
              onBlur={this.handleFormSubmit}
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

    componentDidMount: function() {
      /**
       * OT inserts inline styles into the markup. Using a listener for
       * resize events helps us trigger a full width/height on the element
       * so that they update to the correct dimensions.
       * XXX: this should be factored as a mixin.
       */
      window.addEventListener('orientationchange', this.updateVideoContainer);
      window.addEventListener('resize', this.updateVideoContainer);

      // The SDK needs to know about the configuration and the elements to use
      // for display. So the best way seems to pass the information here - ideally
      // the sdk wouldn't need to know this, but we can't change that.
      this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
        publisherConfig: this._getPublisherConfig(),
        getLocalElementFunc: this._getElement.bind(this, ".local"),
        getRemoteElementFunc: this._getElement.bind(this, ".remote")
      }));
    },

    _getPublisherConfig: function() {
      // height set to 100%" to fix video layout on Google Chrome
      // @see https://bugzilla.mozilla.org/show_bug.cgi?id=1020445
      return {
        insertMode: "append",
        width: "100%",
        height: "100%",
        publishVideo: !this.state.videoMuted,
        style: {
          audioLevelDisplayMode: "off",
          bugDisplayMode: "off",
          buttonDisplayMode: "off",
          nameDisplayMode: "off",
          videoDisabledDisplayMode: "off"
        }
      };
    },

    /**
     * Used to update the video container whenever the orientation or size of the
     * display area changes.
     */
    updateVideoContainer: function() {
      var localStreamParent = this._getElement('.local .OT_publisher');
      var remoteStreamParent = this._getElement('.remote .OT_subscriber');
      if (localStreamParent) {
        localStreamParent.style.width = "100%";
      }
      if (remoteStreamParent) {
        remoteStreamParent.style.height = "100%";
      }
    },

    /**
     * Returns either the required DOMNode
     *
     * @param {String} className The name of the class to get the element for.
     */
    _getElement: function(className) {
      return this.getDOMNode().querySelector(className);
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
        "local-stream-audio": !this.state.videoMuted,
        "room-preview": this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS
      });

      switch(this.state.roomState) {
        case ROOM_STATES.FAILED:
        case ROOM_STATES.FULL: {
          // Note: While rooms are set to hold a maximum of 2 participants, the
          //       FULL case should never happen on desktop.
          return <loop.conversation.GenericFailureView
            cancelCall={this.closeWindow}
          />;
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
                    hangup={noop} />
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
