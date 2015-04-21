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
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
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
      this.listenTo(this.props.roomStore, "change:error",
                    this._onRoomError);
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

    _onRoomError: function() {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState({error: this.props.roomStore.getStoreState("error")});
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

  var SocialShareDropdown = React.createClass({displayName: "SocialShareDropdown",
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      roomUrl: React.PropTypes.string,
      show: React.PropTypes.bool.isRequired,
      socialShareButtonAvailable: React.PropTypes.bool,
      socialShareProviders: React.PropTypes.array
    },

    handleToolbarAddButtonClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.AddSocialShareButton());
    },

    handleAddServiceClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.AddSocialShareProvider());
    },

    handleProviderClick: function(event) {
      event.preventDefault();

      var origin = event.currentTarget.dataset.provider;
      var provider = this.props.socialShareProviders.filter(function(provider) {
        return provider.origin == origin;
      })[0];

      this.props.dispatcher.dispatch(new sharedActions.ShareRoomUrl({
        provider: provider,
        roomUrl: this.props.roomUrl,
        previews: []
      }));
    },

    render: function() {
      // Don't render a thing when no data has been fetched yet.
      if (!this.props.socialShareProviders) {
        return null;
      }

      var cx = React.addons.classSet;
      var shareDropdown = cx({
        "share-service-dropdown": true,
        "dropdown-menu": true,
        "share-button-unavailable": !this.props.socialShareButtonAvailable,
        "hide": !this.props.show
      });

      // When the button is not yet available, we offer to put it in the navbar
      // for the user.
      if (!this.props.socialShareButtonAvailable) {
        return (
          React.createElement("div", {className: shareDropdown}, 
            React.createElement("div", {className: "share-panel-header"}, 
              mozL10n.get("share_panel_header")
            ), 
            React.createElement("div", {className: "share-panel-body"}, 
              
                mozL10n.get("share_panel_body", {
                  brandShortname: mozL10n.get("brandShortname"),
                  clientSuperShortname: mozL10n.get("clientSuperShortname"),
                })
              
            ), 
            React.createElement("button", {className: "btn btn-info btn-toolbar-add", 
                    onClick: this.handleToolbarAddButtonClick}, 
              mozL10n.get("add_to_toolbar_button")
            )
          )
        );
      }

      return (
        React.createElement("ul", {className: shareDropdown}, 
          React.createElement("li", {className: "dropdown-menu-item", onClick: this.handleAddServiceClick}, 
            React.createElement("i", {className: "icon icon-add-share-service"}), 
            React.createElement("span", null, mozL10n.get("share_add_service_button"))
          ), 
          this.props.socialShareProviders.length ? React.createElement("li", {className: "dropdown-menu-separator"}) : null, 
          
            this.props.socialShareProviders.map(function(provider, idx) {
              return (
                React.createElement("li", {className: "dropdown-menu-item", 
                    key: "provider-" + idx, 
                    "data-provider": provider.origin, 
                    onClick: this.handleProviderClick}, 
                  React.createElement("img", {className: "icon", src: provider.iconURL}), 
                  React.createElement("span", null, provider.name)
                )
              );
            }.bind(this))
          
        )
      );
    }
  });

  /**
   * Desktop room invitation view (overlay).
   */
  var DesktopRoomInvitationView = React.createClass({displayName: "DesktopRoomInvitationView",
    mixins: [React.addons.LinkedStateMixin, sharedMixins.DropdownMenuMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      error: React.PropTypes.object,
      // This data is supplied by the activeRoomStore.
      roomData: React.PropTypes.object.isRequired,
      show: React.PropTypes.bool.isRequired,
      showContext: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      return {
        copiedUrl: false,
        newRoomName: ""
      };
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
        roomToken: this.props.roomData.roomToken,
        newRoomName: this.state.newRoomName
      }));
    },

    handleEmailButtonClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(
        new sharedActions.EmailRoomUrl({roomUrl: this.props.roomData.roomUrl}));
    },

    handleCopyButtonClick: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(
        new sharedActions.CopyRoomUrl({roomUrl: this.props.roomData.roomUrl}));

      this.setState({copiedUrl: true});
    },

    handleShareButtonClick: function(event) {
      event.preventDefault();

      this.toggleDropdownMenu();
    },

    render: function() {
      if (!this.props.show) {
        return null;
      }

      var cx = React.addons.classSet;
      return (
        React.createElement("div", {className: "room-invitation-overlay"}, 
          React.createElement("div", {className: "room-invitation-content"}, 
            React.createElement("p", {className: cx({"error": !!this.props.error,
                              "error-display-area": true})}, 
              mozL10n.get("rooms_name_change_failed_label")
            ), 
            React.createElement("form", {onSubmit: this.handleFormSubmit}, 
              React.createElement("textarea", {rows: "2", type: "text", className: "input-room-name", 
                valueLink: this.linkState("newRoomName"), 
                onBlur: this.handleFormSubmit, 
                onKeyDown: this.handleTextareaKeyDown, 
                placeholder: mozL10n.get("rooms_name_this_room_label")})
            ), 
            React.createElement("p", null, mozL10n.get("invite_header_text")), 
            React.createElement("div", {className: "btn-group call-action-group"}, 
              React.createElement("button", {className: "btn btn-info btn-email", 
                      onClick: this.handleEmailButtonClick}, 
                mozL10n.get("email_link_button")
              ), 
              React.createElement("button", {className: "btn btn-info btn-copy", 
                      onClick: this.handleCopyButtonClick}, 
                this.state.copiedUrl ? mozL10n.get("copied_url_button") :
                                        mozL10n.get("copy_url_button2")
              ), 
              React.createElement("button", {className: "btn btn-info btn-share", 
                      ref: "anchor", 
                      onClick: this.handleShareButtonClick}, 
                mozL10n.get("share_button3")
              )
            ), 
            React.createElement(SocialShareDropdown, {dispatcher: this.props.dispatcher, 
                                 roomUrl: this.props.roomData.roomUrl, 
                                 show: this.state.showMenu, 
                                 ref: "menu"})
          ), 
          React.createElement(DesktopRoomContextView, {
            roomData: this.props.roomData, 
            show: this.props.showContext})
        )
      );
    }
  });

  var DesktopRoomContextView = React.createClass({displayName: "DesktopRoomContextView",
    propTypes: {
      // This data is supplied by the activeRoomStore.
      roomData: React.PropTypes.object.isRequired,
      show: React.PropTypes.bool.isRequired
    },

    componentWillReceiveProps: function(nextProps) {
      // When the 'show' prop is changed from outside this component, we do need
      // to update the state.
      if (("show" in nextProps) && nextProps.show !== this.props.show) {
        this.setState({ show: nextProps.show });
      }
    },

    getInitialState: function() {
      return { show: this.props.show };
    },

    handleCloseClick: function() {
      this.setState({ show: false });
    },

    render: function() {
      if (!this.state.show)
        return null;

      var URL = this.props.roomData.roomContextUrls && this.props.roomData.roomContextUrls[0];
      var thumbnail = URL && URL.thumbnail || "";
      var URLDescription = URL && URL.description || "";
      var location = URL && URL.location || "";
      return (
        React.createElement("div", {className: "room-context"}, 
          React.createElement("img", {className: "room-context-thumbnail", src: thumbnail}), 
          React.createElement("div", {className: "room-context-content"}, 
            React.createElement("div", {className: "room-context-label"}, mozL10n.get("context_inroom_label")), 
            React.createElement("div", {className: "room-context-description"}, URLDescription), 
            React.createElement("a", {className: "room-context-url", href: location, target: "_blank"}, location), 
            this.props.roomData.roomDescription ?
              React.createElement("div", {className: "room-context-comment"}, this.props.roomData.roomDescription) :
              null, 
            React.createElement("button", {className: "room-context-btn-close", 
                    onClick: this.handleCloseClick})
          )
        )
      );
    }
  });

  /**
   * Desktop room conversation view.
   */
  var DesktopRoomConversationView = React.createClass({displayName: "DesktopRoomConversationView",
    mixins: [
      ActiveRoomStoreMixin,
      sharedMixins.DocumentTitleMixin,
      sharedMixins.MediaSetupMixin,
      sharedMixins.RoomsAudioMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired
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
          getScreenShareElementFunc: this._getElement.bind(this, ".screen"),
          getRemoteElementFunc: this._getElement.bind(this, ".remote")
        }));
      }
    },

    /**
     * User clicked on the "Leave" button.
     */
    leaveRoom: function() {
      if (this.state.used) {
        this.props.dispatcher.dispatch(new sharedActions.LeaveRoom());
      } else {
        this.closeWindow();
      }
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

    _shouldRenderInvitationOverlay: function() {
      return (this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS);
    },

    _shouldRenderContextView: function() {
      return !!(
        this.props.mozLoop.getLoopPref("contextInConverations.enabled") &&
        (this.state.roomContextUrls || this.state.roomDescription)
      );
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

      var screenShareData = {
        state: this.state.screenSharingState,
        visible: true
      };

      var shouldRenderInvitationOverlay = this._shouldRenderInvitationOverlay();
      var shouldRenderContextView = this._shouldRenderContextView();
      var roomData = this.props.roomStore.getStoreState("activeRoom");

      switch(this.state.roomState) {
        case ROOM_STATES.FAILED:
        case ROOM_STATES.FULL: {
          // Note: While rooms are set to hold a maximum of 2 participants, the
          //       FULL case should never happen on desktop.
          return (
            React.createElement(loop.conversationViews.GenericFailureView, {
              cancelCall: this.closeWindow, 
              failureReason: this.state.failureReason})
          );
        }
        case ROOM_STATES.ENDED: {
          return (
            React.createElement(sharedViews.FeedbackView, {
              onAfterFeedbackReceived: this.closeWindow})
          );
        }
        default: {
          return (
            React.createElement("div", {className: "room-conversation-wrapper"}, 
              React.createElement(DesktopRoomInvitationView, {
                dispatcher: this.props.dispatcher, 
                error: this.state.error, 
                roomData: roomData, 
                show: shouldRenderInvitationOverlay, 
                showContext: shouldRenderContextView}), 
              React.createElement("div", {className: "video-layout-wrapper"}, 
                React.createElement("div", {className: "conversation room-conversation"}, 
                  React.createElement("div", {className: "media nested"}, 
                    React.createElement("div", {className: "video_wrapper remote_wrapper"}, 
                      React.createElement("div", {className: "video_inner remote focus-stream"})
                    ), 
                    React.createElement("div", {className: localStreamClasses}), 
                    React.createElement("div", {className: "screen hide"})
                  ), 
                  React.createElement(sharedViews.ConversationToolbar, {
                    dispatcher: this.props.dispatcher, 
                    video: {enabled: !this.state.videoMuted, visible: true}, 
                    audio: {enabled: !this.state.audioMuted, visible: true}, 
                    publishStream: this.publishStream, 
                    hangup: this.leaveRoom, 
                    screenShare: screenShareData})
                )
              ), 
              React.createElement(DesktopRoomContextView, {
                roomData: roomData, 
                show: !shouldRenderInvitationOverlay && shouldRenderContextView})
            )
          );
        }
      }
    }
  });

  return {
    ActiveRoomStoreMixin: ActiveRoomStoreMixin,
    SocialShareDropdown: SocialShareDropdown,
    DesktopRoomContextView: DesktopRoomContextView,
    DesktopRoomConversationView: DesktopRoomConversationView,
    DesktopRoomInvitationView: DesktopRoomInvitationView
  };

})(document.mozL10n || navigator.mozL10n);
