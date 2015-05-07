/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedUtils = loop.shared.utils;
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

  var SocialShareDropdown = React.createClass({
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
          <div className={shareDropdown}>
            <div className="share-panel-header">
              {mozL10n.get("share_panel_header")}
            </div>
            <div className="share-panel-body">
              {
                mozL10n.get("share_panel_body", {
                  brandShortname: mozL10n.get("brandShortname"),
                  clientSuperShortname: mozL10n.get("clientSuperShortname")
                })
              }
            </div>
            <button className="btn btn-info btn-toolbar-add"
                    onClick={this.handleToolbarAddButtonClick}>
              {mozL10n.get("add_to_toolbar_button")}
            </button>
          </div>
        );
      }

      return (
        <ul className={shareDropdown}>
          <li className="dropdown-menu-item" onClick={this.handleAddServiceClick}>
            <i className="icon icon-add-share-service"></i>
            <span>{mozL10n.get("share_add_service_button")}</span>
          </li>
          {this.props.socialShareProviders.length ? <li className="dropdown-menu-separator"/> : null}
          {
            this.props.socialShareProviders.map(function(provider, idx) {
              return (
                <li className="dropdown-menu-item"
                    key={"provider-" + idx}
                    data-provider={provider.origin}
                    onClick={this.handleProviderClick}>
                  <img className="icon" src={provider.iconURL}/>
                  <span>{provider.name}</span>
                </li>
              );
            }.bind(this))
          }
        </ul>
      );
    }
  });

  /**
   * Desktop room invitation view (overlay).
   */
  var DesktopRoomInvitationView = React.createClass({
    mixins: [sharedMixins.DropdownMenuMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      error: React.PropTypes.object,
      mozLoop: React.PropTypes.object.isRequired,
      // This data is supplied by the activeRoomStore.
      roomData: React.PropTypes.object.isRequired,
      show: React.PropTypes.bool.isRequired,
      showContext: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      return {
        copiedUrl: false,
        editMode: false
      };
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

    handleAddContextClick: function(event) {
      event.preventDefault();

      this.handleEditModeChange(true);
    },

    handleEditModeChange: function(newEditMode) {
      this.setState({ editMode: newEditMode });
    },

    render: function() {
      if (!this.props.show) {
        return null;
      }

      var canAddContext = this.props.mozLoop.getLoopPref("contextInConversations.enabled") &&
        !this.props.showContext && !this.state.editMode;

      var cx = React.addons.classSet;
      return (
        <div className="room-invitation-overlay">
          <div className="room-invitation-content">
            <p className={cx({hide: this.state.editMode})}>
              {mozL10n.get("invite_header_text")}
            </p>
            <a className={cx({hide: !canAddContext, "room-invitation-addcontext": true})}
               onClick={this.handleAddContextClick}>
              {mozL10n.get("context_add_some_label")}
            </a>
            <div className="btn-group call-action-group">
              <button className="btn btn-info btn-email"
                      onClick={this.handleEmailButtonClick}>
                {mozL10n.get("email_link_button")}
              </button>
              <button className="btn btn-info btn-copy"
                      onClick={this.handleCopyButtonClick}>
                {this.state.copiedUrl ? mozL10n.get("copied_url_button") :
                                        mozL10n.get("copy_url_button2")}
              </button>
              <button className="btn btn-info btn-share"
                      ref="anchor"
                      onClick={this.handleShareButtonClick}>
                {mozL10n.get("share_button3")}
              </button>
            </div>
            <SocialShareDropdown
              dispatcher={this.props.dispatcher}
              roomUrl={this.props.roomData.roomUrl}
              show={this.state.showMenu}
              socialShareButtonAvailable={this.props.socialShareButtonAvailable}
              socialShareProviders={this.props.socialShareProviders}
              ref="menu" />
          </div>
          <DesktopRoomContextView
            dispatcher={this.props.dispatcher}
            editMode={this.state.editMode}
            error={this.props.error}
            mozLoop={this.props.mozLoop}
            onEditModeChange={this.handleEditModeChange}
            roomData={this.props.roomData}
            show={this.props.showContext || this.state.editMode} />
        </div>
      );
    }
  });

  var DesktopRoomContextView = React.createClass({
    mixins: [React.addons.LinkedStateMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      editMode: React.PropTypes.bool,
      error: React.PropTypes.object,
      mozLoop: React.PropTypes.object.isRequired,
      onEditModeChange: React.PropTypes.func,
      // This data is supplied by the activeRoomStore.
      roomData: React.PropTypes.object.isRequired,
      show: React.PropTypes.bool.isRequired
    },

    componentWillReceiveProps: function(nextProps) {
      var newState = {};
      // When the 'show' prop is changed from outside this component, we do need
      // to update the state.
      if (("show" in nextProps) && nextProps.show !== this.props.show) {
        newState.show = nextProps.show;
      }
      if (("editMode" in nextProps && nextProps.editMode !== this.props.editMode)) {
        newState.editMode = nextProps.editMode;
        // If we're switching to edit mode, fetch the metadata of the current tab.
        // But _only_ if there's no context currently attached to the room; the
        // checkbox will be disabled in that case.
        if (nextProps.editMode) {
          this.props.mozLoop.getSelectedTabMetadata(function(metadata) {
            var previewImage = metadata.previews.length ? metadata.previews[0] : "";
            var description = metadata.description || metadata.title;
            var url = metadata.url;
            this.setState({
              availableContext: {
                previewImage: previewImage,
                description: description,
                url: url
              }
           });
          }.bind(this));
        }
      }
      // When we receive an update for the `roomData` property, make sure that
      // the current form fields reflect reality. This is necessary, because the
      // form state is maintained in the components' state.
      if (nextProps.roomData) {
        // Right now it's only necessary to update the form input states when
        // they contain no text yet.
        if (!this.state.newRoomName && nextProps.roomData.roomName) {
          newState.newRoomName = nextProps.roomData.roomName;
        }
        var url = this._getURL(nextProps.roomData);
        if (url) {
          if (!this.state.newRoomURL && url.location) {
            newState.newRoomURL = url.location;
          }
          if (!this.state.newRoomDescription && url.description) {
            newState.newRoomDescription = url.description;
          }
          if (!this.state.newRoomThumbnail && url.thumbnail) {
            newState.newRoomThumbnail = url.thumbnail;
          }
        }
      }

      if (Object.getOwnPropertyNames(newState).length) {
        this.setState(newState);
      }
    },

    getDefaultProps: function() {
      return { editMode: false };
     },

    getInitialState: function() {
      var url = this._getURL();
      return {
        // `availableContext` prop only used in tests.
        availableContext: this.props.availableContext,
        editMode: this.props.editMode,
        show: this.props.show,
        newRoomName: this.props.roomData.roomName || "",
        newRoomURL: url && url.location || "",
        newRoomDescription: url && url.description || "",
        newRoomThumbnail: url && url.thumbnail || ""
      };
    },

    handleCloseClick: function(event) {
      event.preventDefault();

      if (this.state.editMode) {
        this.setState({ editMode: false });
        if (this.props.onEditModeChange) {
          this.props.onEditModeChange(false);
        }
        return;
      }
      this.setState({ show: false });
    },

    handleEditClick: function(event) {
      event.preventDefault();

      this.setState({ editMode: true });
      if (this.props.onEditModeChange) {
        this.props.onEditModeChange(true);
      }
    },

    handleCheckboxChange: function(state) {
      if (state.checked) {
        // The checkbox was checked, prefill the fields with the values available
        // in `availableContext`.
        var context = this.state.availableContext;
        this.setState({
          newRoomURL: context.url,
          newRoomDescription: context.description,
          newRoomThumbnail: context.previewImage
        }, this.handleFormSubmit);
      } else {
        this.setState({
          newRoomURL: "",
          newRoomDescription: "",
          newRoomThumbnail: ""
        }, this.handleFormSubmit);
      }
    },

    handleFormSubmit: function(event) {
      event && event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: this.props.roomData.roomToken,
        newRoomName: this.state.newRoomName,
        newRoomURL: this.state.newRoomURL,
        newRoomDescription: this.state.newRoomDescription,
        newRoomThumbnail: this.state.newRoomThumbnail
      }));
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

    /**
     * Utility function to extract URL context data from the `roomData` property
     * that can also be supplied as an argument.
     *
     * @param  {Object} roomData Optional room data object to use, equivalent to
     *                           the activeRoomStore state.
     * @return {Object} The first context URL found on the `roomData` object.
     */
    _getURL: function(roomData) {
      roomData = roomData || this.props.roomData;
      return this.props.roomData.roomContextUrls &&
        this.props.roomData.roomContextUrls[0];
    },

    /**
     * Truncate a string if it exceeds the length as defined in `maxLen`, which
     * is defined as '72' characters by default. If the string needs trimming,
     * it'll be suffixed with the unicode ellipsis char, \u2026.
     *
     * @param  {String} str    The string to truncate, if needed.
     * @param  {Number} maxLen Maximum number of characters that the string is
     *                         allowed to contain. Optional, defaults to 72.
     * @return {String} Truncated version of `str`.
     */
    _truncate: function(str, maxLen) {
      if (!maxLen) {
        maxLen = 72;
      }
      return (str.length > maxLen) ? str.substr(0, maxLen) + "…" : str;
    },

    render: function() {
      if (!this.state.show && !this.state.editMode)
        return null;

      var url = this._getURL();
      var thumbnail = url && url.thumbnail || "";
      var urlDescription = url && url.description || "";
      var location = url && url.location || "";
      var checkboxLabel = null;
      var locationData = null;
      if (location) {
        locationData = checkboxLabel = sharedUtils.formatURL(location);
      }
      if (!checkboxLabel) {
        checkboxLabel = sharedUtils.formatURL((this.state.availableContext ?
          this.state.availableContext.url : ""));
      }

      var cx = React.addons.classSet;
      if (this.state.editMode) {
        return (
          <div className="room-context">
            <div className="room-context-content">
              <p className={cx({"error": !!this.props.error,
                                "error-display-area": true})}>
                {mozL10n.get("rooms_change_failed_label")}
              </p>
              <div className="room-context-label">{mozL10n.get("context_inroom_label")}</div>
              <sharedViews.Checkbox
                checked={!!url}
                disabled={!!url || !checkboxLabel}
                label={mozL10n.get("context_edit_activate_label", {
                  title: checkboxLabel ? checkboxLabel.hostname : ""
                })}
                onChange={this.handleCheckboxChange}
                value={location} />
              <form onSubmit={this.handleFormSubmit}>
                <textarea rows="2" type="text" className="room-context-name"
                  onBlur={this.handleFormSubmit}
                  onKeyDown={this.handleTextareaKeyDown}
                  placeholder={mozL10n.get("context_edit_name_placeholder")}
                  valueLink={this.linkState("newRoomName")} />
                <input type="text" className="room-context-url"
                  onBlur={this.handleFormSubmit}
                  onKeyDown={this.handleTextareaKeyDown}
                  placeholder="https://"
                  valueLink={this.linkState("newRoomURL")} />
                <textarea rows="4" type="text" className="room-context-comments"
                  onBlur={this.handleFormSubmit}
                  onKeyDown={this.handleTextareaKeyDown}
                  placeholder={mozL10n.get("context_edit_comments_placeholder")}
                  valueLink={this.linkState("newRoomDescription")} />
              </form>
              <button className="room-context-btn-close"
                      onClick={this.handleCloseClick}/>
            </div>
          </div>
        );
      }

      if (!locationData) {
        return null;
      }

      return (
        <div className="room-context">
          <img className="room-context-thumbnail" src={thumbnail}/>
          <div className="room-context-content">
            <div className="room-context-label">{mozL10n.get("context_inroom_label")}</div>
            <div className="room-context-description"
                 title={urlDescription}>{this._truncate(urlDescription)}</div>
            <a className="room-context-url"
               href={location}
               target="_blank"
               title={locationData.location}>{locationData.hostname}</a>
            {this.props.roomData.roomDescription ?
              <div className="room-context-comment">{this.props.roomData.roomDescription}</div> :
              null}
            <button className="room-context-btn-close"
                    onClick={this.handleCloseClick}/>
            <button className="room-context-btn-edit"
                    onClick={this.handleEditClick}/>
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
        this.props.mozLoop.getLoopPref("contextInConversations.enabled") &&
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
            <loop.conversationViews.GenericFailureView
              cancelCall={this.closeWindow}
              failureReason={this.state.failureReason} />
          );
        }
        case ROOM_STATES.ENDED: {
          return (
            <sharedViews.FeedbackView
              onAfterFeedbackReceived={this.closeWindow} />
          );
        }
        default: {
          return (
            <div className="room-conversation-wrapper">
              <DesktopRoomInvitationView
                dispatcher={this.props.dispatcher}
                error={this.state.error}
                mozLoop={this.props.mozLoop}
                roomData={roomData}
                show={shouldRenderInvitationOverlay}
                showContext={shouldRenderContextView}
                socialShareButtonAvailable={this.state.socialShareButtonAvailable}
                socialShareProviders={this.state.socialShareProviders} />
              <div className="video-layout-wrapper">
                <div className="conversation room-conversation">
                  <div className="media nested">
                    <div className="video_wrapper remote_wrapper">
                      <div className="video_inner remote focus-stream"></div>
                    </div>
                    <div className={localStreamClasses}></div>
                    <div className="screen hide"></div>
                  </div>
                  <sharedViews.ConversationToolbar
                    dispatcher={this.props.dispatcher}
                    video={{enabled: !this.state.videoMuted, visible: true}}
                    audio={{enabled: !this.state.audioMuted, visible: true}}
                    publishStream={this.publishStream}
                    hangup={this.leaveRoom}
                    screenShare={screenShareData} />
                </div>
              </div>
              <DesktopRoomContextView
                dispatcher={this.props.dispatcher}
                error={this.state.error}
                mozLoop={this.props.mozLoop}
                roomData={roomData}
                show={!shouldRenderInvitationOverlay && shouldRenderContextView} />
            </div>
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
