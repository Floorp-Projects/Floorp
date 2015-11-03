/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.panel = (function(_, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views;
  var sharedModels = loop.shared.models;
  var sharedMixins = loop.shared.mixins;
  var sharedActions = loop.shared.actions;
  var Button = sharedViews.Button;
  var Checkbox = sharedViews.Checkbox;

  var GettingStartedView = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      mozLoop: React.PropTypes.object.isRequired
    },

    handleButtonClick: function() {
      navigator.mozLoop.openGettingStartedTour("getting-started");
      navigator.mozLoop.setLoopPref("gettingStarted.seen", true);
      var event = new CustomEvent("GettingStartedSeen");
      window.dispatchEvent(event);
      this.closeWindow();
    },

    render: function() {
      if (this.props.mozLoop.getLoopPref("gettingStarted.seen")) {
        return null;
      }
      return (
        <div className="fte-get-started-content">
          <header className="fte-title">
            <img src="loop/shared/img/hello_logo.svg" />
            <div className="fte-subheader">
              {mozL10n.get("first_time_experience_subheading")}
            </div>
          </header>
          <Button additionalClass="fte-get-started-button"
                  caption={mozL10n.get("first_time_experience_button_label")}
                  htmlId="fte-button"
                  onClick={this.handleButtonClick} />
        </div>
      );
    }
  });

  /**
   * Displays a view requesting the user to sign-in again.
   */
  var SignInRequestView = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      mozLoop: React.PropTypes.object.isRequired
    },

    handleSignInClick: function(event) {
      event.preventDefault();
      this.props.mozLoop.logInToFxA(true);
      this.closeWindow();
    },

    handleGuestClick: function(event) {
      this.props.mozLoop.logOutFromFxA();
    },

    render: function() {
      var shortname = mozL10n.get("clientShortname2");
      var line1 = mozL10n.get("sign_in_again_title_line_one", {
        clientShortname2: shortname
      });
      var line2 = mozL10n.get("sign_in_again_title_line_two2", {
        clientShortname2: shortname
      });
      var useGuestString = mozL10n.get("sign_in_again_use_as_guest_button2", {
        clientSuperShortname: mozL10n.get("clientSuperShortname")
      });

      return (
        <div className="sign-in-request">
          <h1>{line1}</h1>
          <h2>{line2}</h2>
          <div>
            <button className="btn btn-info sign-in-request-button"
                    onClick={this.handleSignInClick}>
              {mozL10n.get("sign_in_again_button")}
            </button>
          </div>
          <a onClick={this.handleGuestClick}>
            {useGuestString}
          </a>
        </div>
      );
    }
  });

  var ToSView = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      mozLoop: React.PropTypes.object.isRequired
    },

    handleLinkClick: function(event) {
      if (!event.target || !event.target.href) {
        return;
      }

      event.preventDefault();
      this.props.mozLoop.openURL(event.target.href);
      this.closeWindow();
    },

    render: function() {
      var locale = mozL10n.getLanguage();
      var terms_of_use_url = this.props.mozLoop.getLoopPref("legal.ToS_url");
      var privacy_notice_url = this.props.mozLoop.getLoopPref("legal.privacy_url");
      var tosHTML = mozL10n.get("legal_text_and_links3", {
        "clientShortname": mozL10n.get("clientShortname2"),
        "terms_of_use": React.renderToStaticMarkup(
          <a href={terms_of_use_url} target="_blank">
            {mozL10n.get("legal_text_tos")}
          </a>
        ),
        "privacy_notice": React.renderToStaticMarkup(
          <a href={privacy_notice_url} target="_blank">
            {mozL10n.get("legal_text_privacy")}
          </a>
        )
      });

      return (
        <div className="powered-by-wrapper" id="powered-by-wrapper">
          <p className="powered-by" id="powered-by">
            {mozL10n.get("powered_by_beforeLogo")}
            <span className={locale} id="powered-by-logo"/>
            {mozL10n.get("powered_by_afterLogo")}
          </p>
          <p className="terms-service"
             dangerouslySetInnerHTML={{ __html: tosHTML }}
             onClick={this.handleLinkClick}></p>
         </div>
      );
    }
  });

  /**
   * Panel settings (gear) menu entry.
   */
  var SettingsDropdownEntry = React.createClass({
    propTypes: {
      displayed: React.PropTypes.bool,
      extraCSSClass: React.PropTypes.string,
      label: React.PropTypes.string.isRequired,
      onClick: React.PropTypes.func.isRequired
    },

    getDefaultProps: function() {
      return { displayed: true };
    },

    render: function() {
      var cx = React.addons.classSet;

      if (!this.props.displayed) {
        return null;
      }

      var extraCSSClass = {
        "dropdown-menu-item": true
      };
      if (this.props.extraCSSClass) {
        extraCSSClass[this.props.extraCSSClass] = true;
      }

      return (
        <li className={cx(extraCSSClass)} onClick={this.props.onClick}>
          {this.props.label}
        </li>
      );
    }
  });

  /**
   * Panel settings (gear) menu.
   */
  var SettingsDropdown = React.createClass({
    propTypes: {
      mozLoop: React.PropTypes.object.isRequired
    },

    mixins: [sharedMixins.DropdownMenuMixin(), sharedMixins.WindowCloseMixin],

    handleClickSettingsEntry: function() {
      // XXX to be implemented at the same time as unhiding the entry
    },

    handleClickAccountEntry: function() {
      this.props.mozLoop.openFxASettings();
      this.closeWindow();
    },

    handleClickAuthEntry: function() {
      if (this._isSignedIn()) {
        this.props.mozLoop.logOutFromFxA();
      } else {
        this.props.mozLoop.logInToFxA();
      }
    },

    handleHelpEntry: function(event) {
      event.preventDefault();
      var helloSupportUrl = this.props.mozLoop.getLoopPref("support_url");
      this.props.mozLoop.openURL(helloSupportUrl);
      this.closeWindow();
    },

    handleToggleNotifications: function() {
      this.props.mozLoop.doNotDisturb = !this.props.mozLoop.doNotDisturb;
      this.hideDropdownMenu();
    },

    /**
     * Load on the browser the feedback url from prefs
     */
    handleSubmitFeedback: function(event) {
      event.preventDefault();
      var helloFeedbackUrl = this.props.mozLoop.getLoopPref("feedback.formURL");
      this.props.mozLoop.openURL(helloFeedbackUrl);
      this.closeWindow();
    },

    _isSignedIn: function() {
      return !!this.props.mozLoop.userProfile;
    },

    openGettingStartedTour: function() {
      this.props.mozLoop.openGettingStartedTour("settings-menu");
      this.closeWindow();
    },

    render: function() {
      var cx = React.addons.classSet;
      var accountEntryCSSClass = this._isSignedIn() ? "entry-settings-signout" :
                                                      "entry-settings-signin";
      var notificationsLabel = this.props.mozLoop.doNotDisturb ? "settings_menu_item_turnnotificationson" :
                                                                 "settings_menu_item_turnnotificationsoff";

      return (
        <div className="settings-menu dropdown">
          <button className="button-settings"
             onClick={this.toggleDropdownMenu}
             ref="menu-button"
             title={mozL10n.get("settings_menu_button_tooltip")} />
          <ul className={cx({ "dropdown-menu": true, hide: !this.state.showMenu })}>
            <SettingsDropdownEntry
                extraCSSClass="entry-settings-notifications entries-divider"
                label={mozL10n.get(notificationsLabel)}
                onClick={this.handleToggleNotifications} />
            <SettingsDropdownEntry
                displayed={this._isSignedIn() && this.props.mozLoop.fxAEnabled}
                extraCSSClass="entry-settings-account"
                label={mozL10n.get("settings_menu_item_account")}
                onClick={this.handleClickAccountEntry} />
            <SettingsDropdownEntry displayed={false}
                                   label={mozL10n.get("settings_menu_item_settings")}
                                   onClick={this.handleClickSettingsEntry} />
            <SettingsDropdownEntry label={mozL10n.get("tour_label")}
                                   onClick={this.openGettingStartedTour} />
            <SettingsDropdownEntry extraCSSClass="entry-settings-feedback"
                                   label={mozL10n.get("settings_menu_item_feedback")}
                                   onClick={this.handleSubmitFeedback} />
            <SettingsDropdownEntry displayed={this.props.mozLoop.fxAEnabled}
                                   extraCSSClass={accountEntryCSSClass}
                                   label={this._isSignedIn() ?
                                          mozL10n.get("settings_menu_item_signout") :
                                          mozL10n.get("settings_menu_item_signin")}
                                   onClick={this.handleClickAuthEntry} />
            <SettingsDropdownEntry extraCSSClass="entry-settings-help"
                                   label={mozL10n.get("help_label")}
                                   onClick={this.handleHelpEntry} />
          </ul>
        </div>
      );
    }
  });

  /**
   * FxA sign in/up link component.
   */
  var AccountLink = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      fxAEnabled: React.PropTypes.bool.isRequired,
      userProfile: userProfileValidator
    },

    handleSignInLinkClick: function() {
      navigator.mozLoop.logInToFxA();
      this.closeWindow();
    },

    render: function() {
      if (!this.props.fxAEnabled) {
        return null;
      }

      if (this.props.userProfile && this.props.userProfile.email) {
        return (
          <div className="user-identity">
            {loop.shared.utils.truncate(this.props.userProfile.email, 24)}
          </div>
        );
      }

      return (
        <p className="signin-link">
          <a href="#" onClick={this.handleSignInLinkClick}>
            {mozL10n.get("panel_footer_signin_or_signup_link")}
          </a>
        </p>
      );
    }
  });

  var RoomEntryContextItem = React.createClass({
    mixins: [loop.shared.mixins.WindowCloseMixin],

    propTypes: {
      mozLoop: React.PropTypes.object.isRequired,
      roomUrls: React.PropTypes.array
    },

    handleClick: function(event) {
      event.stopPropagation();
      event.preventDefault();
      if (event.currentTarget.href) {
        this.props.mozLoop.openURL(event.currentTarget.href);
        this.closeWindow();
      }
    },

    _renderDefaultIcon: function() {
      return (
        <div className="room-entry-context-item">
          <img src="loop/shared/img/icons-16x16.svg#globe" />
        </div>
      );
    },

    _renderIcon: function(roomUrl) {
      return (
        <div className="room-entry-context-item">
          <a href={roomUrl.location}
            onClick={this.handleClick}
            title={roomUrl.description}>
            <img src={roomUrl.thumbnail || "loop/shared/img/icons-16x16.svg#globe"} />
          </a>
        </div>
      );
    },

    render: function() {
      var roomUrl = this.props.roomUrls && this.props.roomUrls[0];
      if (roomUrl && roomUrl.location) {
        return this._renderIcon(roomUrl);
      } else {
        return this._renderDefaultIcon();
      }
    }
  });

  /**
   * Room list entry.
   *
   * Active Room means there are participants in the room.
   * Opened Room means the user is in the room.
   */
  var RoomEntry = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      isOpenedRoom: React.PropTypes.bool.isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      room: React.PropTypes.instanceOf(loop.store.Room).isRequired
    },

    mixins: [
      loop.shared.mixins.WindowCloseMixin,
      sharedMixins.DropdownMenuMixin()
    ],

    getInitialState: function() {
      return {
        eventPosY: 0
      };
    },

    _isActive: function() {
      return this.props.room.participants.length > 0;
    },

    handleClickEntry: function(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.OpenRoom({
        roomToken: this.props.room.roomToken
      }));
      this.closeWindow();
    },

    handleClick: function(e) {
      e.preventDefault();
      e.stopPropagation();

      this.setState({
        eventPosY: e.pageY
      });

      this.toggleDropdownMenu();
    },

    /**
     * Callback called when moving cursor away from the conversation entry.
     * Will close the dropdown menu.
     */
    _handleMouseOut: function() {
      if (this.state.showMenu) {
        this.toggleDropdownMenu();
      }
    },

    render: function() {
      var roomClasses = React.addons.classSet({
        "room-entry": true,
        "room-active": this._isActive(),
        "room-opened": this.props.isOpenedRoom
      });

      var roomTitle = this.props.room.decryptedContext.roomName ||
        this.props.room.decryptedContext.urls[0].description ||
        this.props.room.decryptedContext.urls[0].location;

      return (
        <div className={roomClasses}
          onClick={this.props.isOpenedRoom ? null : this.handleClickEntry}
          onMouseLeave={this.props.isOpenedRoom ? null : this._handleMouseOut}
          ref="roomEntry">
          <RoomEntryContextItem
            mozLoop={this.props.mozLoop}
            roomUrls={this.props.room.decryptedContext.urls} />
          <h2>{roomTitle}</h2>
          {this.props.isOpenedRoom ? null :
            <RoomEntryContextButtons
              dispatcher={this.props.dispatcher}
              eventPosY={this.state.eventPosY}
              handleClick={this.handleClick}
              ref="contextActions"
              room={this.props.room}
              showMenu={this.state.showMenu}
              toggleDropdownMenu={this.toggleDropdownMenu} />}
        </div>
      );
    }
  });

  /**
   * Buttons corresponding to each conversation entry.
   * This component renders the edit button for displaying contextual dropdown
   * menu for conversation entries. It also holds the dropdown menu.
   */
  var RoomEntryContextButtons = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.object.isRequired,
      eventPosY: React.PropTypes.number.isRequired,
      handleClick: React.PropTypes.func.isRequired,
      room: React.PropTypes.object.isRequired,
      showMenu: React.PropTypes.bool.isRequired,
      toggleDropdownMenu: React.PropTypes.func.isRequired
    },

    handleEmailButtonClick: function(event) {
      event.preventDefault();
      event.stopPropagation();

      this.props.dispatcher.dispatch(
        new sharedActions.EmailRoomUrl({
          roomUrl: this.props.room.roomUrl,
          from: "panel"
        })
      );

      this.props.toggleDropdownMenu();
    },

    handleCopyButtonClick: function(event) {
      event.stopPropagation();
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.CopyRoomUrl({
        roomUrl: this.props.room.roomUrl,
        from: "panel"
      }));

      this.props.toggleDropdownMenu();
    },

    handleDeleteButtonClick: function(event) {
      event.stopPropagation();
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.DeleteRoom({
        roomToken: this.props.room.roomToken
      }));

      this.props.toggleDropdownMenu();
    },

    render: function() {
      return (
        <div className="room-entry-context-actions">
          <div
            className="room-entry-context-edit-btn dropdown-menu-button"
            onClick={this.props.handleClick}
            ref="menu-button" />
          {this.props.showMenu ?
            <ConversationDropdown
              eventPosY={this.props.eventPosY}
              handleCopyButtonClick={this.handleCopyButtonClick}
              handleDeleteButtonClick={this.handleDeleteButtonClick}
              handleEmailButtonClick={this.handleEmailButtonClick}
              ref="menu" /> :
            null}
        </div>
      );
    }
  });

  /**
   * Dropdown menu for each conversation entry.
   * Because the container element has overflow we need to position the menu
   * absolutely and have a different element as offset parent for it. We need
   * eventPosY to make sure the position on the Y Axis is correct while for the
   * X axis there can be only 2 different positions based on being RTL or not.
   */
  var ConversationDropdown = React.createClass({
    propTypes: {
      eventPosY: React.PropTypes.number.isRequired,
      handleCopyButtonClick: React.PropTypes.func.isRequired,
      handleDeleteButtonClick: React.PropTypes.func.isRequired,
      handleEmailButtonClick: React.PropTypes.func.isRequired
    },

    getInitialState: function() {
      return {
        openDirUp: false
      };
    },

    componentDidMount: function() {
      var menuNode = this.getDOMNode();
      var menuNodeRect = menuNode.getBoundingClientRect();

      // Get the parent element and make sure the menu does not overlow its
      // container.
      var listNode = loop.shared.utils.findParentNode(this.getDOMNode(),
                                                      "rooms");
      var listNodeRect = listNode.getBoundingClientRect();

      // Click offset to not display the menu right next to the area clicked.
      var offset = 10;

      if (this.props.eventPosY + menuNodeRect.height >=
          listNodeRect.top + listNodeRect.height) {
        // Position above click area.
        menuNode.style.top = this.props.eventPosY - menuNodeRect.height -
                             listNodeRect.top - offset + "px";
      } else {
        // Position below click area.
        menuNode.style.top = this.props.eventPosY - listNodeRect.top +
                             offset + "px";
      }
    },

    render: function() {
      var dropdownClasses = React.addons.classSet({
        "dropdown-menu": true,
        "dropdown-menu-up": this.state.openDirUp
      });

      return (
        <ul className={dropdownClasses}>
          <li
            className="dropdown-menu-item"
            onClick={this.props.handleCopyButtonClick}
            ref="copyButton">
            {mozL10n.get("copy_link_menuitem")}
          </li>
          <li
            className="dropdown-menu-item"
            onClick={this.props.handleEmailButtonClick}
            ref="emailButton">
            {mozL10n.get("email_link_menuitem")}
          </li>
          <li
            className="dropdown-menu-item"
            onClick={this.props.handleDeleteButtonClick}
            ref="deleteButton">
            {mozL10n.get("delete_conversation_menuitem")}
          </li>
        </ul>
      );
    }
  });

  /**
   * User profile prop can be either an object or null as per mozLoopAPI
   * and there is no way to express this with React 0.12.2
   */
  function userProfileValidator(props, propName, componentName) {
    if (Object.prototype.toString.call(props[propName]) !== "[object Object]" &&
        !_.isNull(props[propName])) {
      return new Error("Required prop `" + propName +
        "` was not correctly specified in `" + componentName + "`.");
    }
  }

  /**
   * Room list.
   */
  var RoomList = React.createClass({
    mixins: [Backbone.Events, sharedMixins.WindowCloseMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      store: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    getInitialState: function() {
      return this.props.store.getStoreState();
    },

    componentDidMount: function() {
      this.listenTo(this.props.store, "change", this._onStoreStateChanged);

      // XXX this should no longer be necessary once have a better mechanism
      // for updating the list (possibly as part of the content side of bug
      // 1074665.
      this.props.dispatcher.dispatch(new sharedActions.GetAllRooms());
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.store);
    },

    componentWillUpdate: function(nextProps, nextState) {
      // If we've just created a room, close the panel - the store will open
      // the room.
      if (this.state.pendingCreation &&
          !nextState.pendingCreation && !nextState.error) {
        this.closeWindow();
      }
    },

    _onStoreStateChanged: function() {
      this.setState(this.props.store.getStoreState());
    },

    /**
     * Let the user know we're loading rooms
     * @returns {Object} React render
     */
    _renderLoadingRoomsView: function() {
      return (
        <div className="room-list">
          {this._renderNewRoomButton()}
          <div className="room-list-loading">
            <img src="loop/shared/img/animated-spinner.svg" />
          </div>
        </div>
      );
    },

    _renderNoRoomsView: function() {
      return (
        <div className="rooms">
          {this._renderNewRoomButton()}
          <div className="room-list-empty">
            <div className="no-conversations-message">
              <p className="panel-text-medium">
                {mozL10n.get("no_conversations_message_heading2")}
              </p>
              <p className="panel-text-medium">
                {mozL10n.get("no_conversations_start_message2")}
              </p>
            </div>
          </div>
        </div>
      );
    },

    _renderNewRoomButton: function() {
      return (
        <NewRoomView dispatcher={this.props.dispatcher}
          inRoom={this.state.openedRoom !== null}
          mozLoop={this.props.mozLoop}
          pendingOperation={this.state.pendingCreation ||
                            this.state.pendingInitialRetrieval} />
      );
    },

    render: function() {
      if (this.state.error) {
        // XXX Better end user reporting of errors.
        console.error("RoomList error", this.state.error);
      }

      if (this.state.pendingInitialRetrieval) {
        return this._renderLoadingRoomsView();
      }

      if (!this.state.rooms.length) {
        return this._renderNoRoomsView();
      }

      return (
        <div className="rooms">
          {this._renderNewRoomButton()}
          <h1>{mozL10n.get(this.state.openedRoom === null ?
                "rooms_list_all_conversations_header" :
                "rooms_list_opened_conversation_header")}</h1>
          <div className="room-list">{
            this.state.rooms.map(function(room, i) {
              if (this.state.openedRoom !== null &&
                room.roomToken !== this.state.openedRoom) {
                return null;
              }

              return (
                <RoomEntry
                  dispatcher={this.props.dispatcher}
                  isOpenedRoom={room.roomToken === this.state.openedRoom}
                  key={room.roomToken}
                  mozLoop={this.props.mozLoop}
                  room={room} />
              );
            }, this)
          }</div>
        </div>
      );
    }
  });

  /**
   * Used for creating a new room with or without context.
   */
  var NewRoomView = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      inRoom: React.PropTypes.bool.isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      pendingOperation: React.PropTypes.bool.isRequired
    },

    mixins: [
      sharedMixins.DocumentVisibilityMixin,
      React.addons.PureRenderMixin
    ],

    getInitialState: function() {
      return {
        checked: false,
        previewImage: "",
        description: "",
        url: ""
      };
    },

    onDocumentVisible: function() {
      // We would use onDocumentHidden to null out the data ready for the next
      // opening. However, this seems to cause an awkward glitch in the display
      // when opening the panel, and it seems cleaner just to update the data
      // even if there's a small delay.

      this.props.mozLoop.getSelectedTabMetadata(function callback(metadata) {
        // Bail out when the component is not mounted (anymore).
        // This occurs during test runs. See bug 1174611 for more info.
        if (!this.isMounted()) {
          return;
        }

        var previewImage = metadata.favicon || "";
        var description = metadata.title || metadata.description;
        var url = metadata.url;
        this.setState({
          checked: false,
          previewImage: previewImage,
          description: description,
          url: url
        });
      }.bind(this));
    },

    onCheckboxChange: function(newState) {
      this.setState({ checked: newState.checked });
    },

    handleCreateButtonClick: function() {
      var createRoomAction = new sharedActions.CreateRoom({
        nameTemplate: mozL10n.get("rooms_default_room_name_template")
      });

      if (this.state.checked) {
        createRoomAction.urls = [{
          location: this.state.url,
          description: this.state.description,
          thumbnail: this.state.previewImage
        }];
      }
      this.props.dispatcher.dispatch(createRoomAction);
    },

    handleStopSharingButtonClick: function() {
      this.props.mozLoop.hangupAllChatWindows();
    },

    render: function() {
      var hostname;

      try {
        hostname = new URL(this.state.url).hostname;
      } catch (ex) {
        // Empty catch - if there's an error, then we won't show the context.
      }

      var parentClasses = React.addons.classSet({
        "context-checkbox-checked": this.state.checked,
        "new-room-view": true
      });
      var contextClasses = React.addons.classSet({
        context: true,
        hide: !hostname ||
          !this.props.mozLoop.getLoopPref("contextInConversations.enabled")
      });

      return (
        <div className={parentClasses}>
          {this.props.inRoom ? null :
            <div className={contextClasses}>
              <Checkbox checked={this.state.checked}
                        label={mozL10n.get("context_inroom_label2")}
                        onChange={this.onCheckboxChange} />
              <sharedViews.ContextUrlView
                allowClick={false}
                description={this.state.description}
                showContextTitle={false}
                thumbnail={this.state.previewImage}
                url={this.state.url}
                useDesktopPaths={true} />
            </div>}
          {this.props.inRoom ?
            <button className="btn btn-info stop-sharing-button"
              disabled={this.props.pendingOperation}
              onClick={this.handleStopSharingButtonClick}>
              {mozL10n.get("panel_end_conversation_button")}
            </button> :
            <button className="btn btn-info new-room-button"
              disabled={this.props.pendingOperation}
              onClick={this.handleCreateButtonClick}>
              {mozL10n.get("panel_start_conversation_button")}
            </button>}
        </div>
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      notifications: React.PropTypes.object.isRequired,
      roomStore:
        React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    getInitialState: function() {
      return {
        hasEncryptionKey: this.props.mozLoop.hasEncryptionKey,
        userProfile: this.props.mozLoop.userProfile,
        gettingStartedSeen: this.props.mozLoop.getLoopPref("gettingStarted.seen")
      };
    },

    _serviceErrorToShow: function() {
      if (!this.props.mozLoop.errors ||
          !Object.keys(this.props.mozLoop.errors).length) {
        return null;
      }
      // Just get the first error for now since more than one should be rare.
      var firstErrorKey = Object.keys(this.props.mozLoop.errors)[0];
      return {
        type: firstErrorKey,
        error: this.props.mozLoop.errors[firstErrorKey]
      };
    },

    updateServiceErrors: function() {
      var serviceError = this._serviceErrorToShow();
      if (serviceError) {
        this.props.notifications.set({
          id: "service-error",
          level: "error",
          message: serviceError.error.friendlyMessage,
          details: serviceError.error.friendlyDetails,
          detailsButtonLabel: serviceError.error.friendlyDetailsButtonLabel,
          detailsButtonCallback: serviceError.error.friendlyDetailsButtonCallback
        });
      } else {
        this.props.notifications.remove(this.props.notifications.get("service-error"));
      }
    },

    _onStatusChanged: function() {
      var profile = this.props.mozLoop.userProfile;
      var currUid = this.state.userProfile ? this.state.userProfile.uid : null;
      var newUid = profile ? profile.uid : null;
      if (currUid === newUid) {
        // Update the state of hasEncryptionKey as this might have changed now.
        this.setState({ hasEncryptionKey: this.props.mozLoop.hasEncryptionKey });
      } else {
        this.setState({ userProfile: profile });
      }
      this.updateServiceErrors();
    },

    _gettingStartedSeen: function() {
      this.setState({
        gettingStartedSeen: this.props.mozLoop.getLoopPref("gettingStarted.seen")
      });
    },

    componentWillMount: function() {
      this.updateServiceErrors();
    },

    componentDidMount: function() {
      window.addEventListener("LoopStatusChanged", this._onStatusChanged);
      window.addEventListener("GettingStartedSeen", this._gettingStartedSeen);
    },

    componentWillUnmount: function() {
      window.removeEventListener("LoopStatusChanged", this._onStatusChanged);
      window.removeEventListener("GettingStartedSeen", this._gettingStartedSeen);
    },

    render: function() {
      var NotificationListView = sharedViews.NotificationListView;

      if (!this.state.gettingStartedSeen) {
        return (
          <div className="fte-get-started-container">
            <NotificationListView
              clearOnDocumentHidden={true}
              notifications={this.props.notifications} />
            <GettingStartedView mozLoop={this.props.mozLoop} />
            <ToSView mozLoop={this.props.mozLoop} />
          </div>
        );
      }

      if (!this.state.hasEncryptionKey) {
        return <SignInRequestView mozLoop={this.props.mozLoop} />;
      }

      return (
        <div className="panel-content">
          <NotificationListView
            clearOnDocumentHidden={true}
            notifications={this.props.notifications} />
            <RoomList dispatcher={this.props.dispatcher}
              mozLoop={this.props.mozLoop}
              store={this.props.roomStore} />
          <div className="footer">
            <div className="user-details">
              <AccountLink fxAEnabled={this.props.mozLoop.fxAEnabled}
                           userProfile={this.state.userProfile}/>
            </div>
            <div className="signin-details">
              <SettingsDropdown mozLoop={this.props.mozLoop}/>
            </div>
          </div>
        </div>
      );
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    var notifications = new sharedModels.NotificationCollection();
    var dispatcher = new loop.Dispatcher();
    var roomStore = new loop.store.RoomStore(dispatcher, {
      mozLoop: navigator.mozLoop,
      notifications: notifications
    });

    React.render(<PanelView
      dispatcher={dispatcher}
      mozLoop={navigator.mozLoop}
      notifications={notifications}
      roomStore={roomStore} />, document.querySelector("#main"));

    document.documentElement.setAttribute("lang", mozL10n.getLanguage());
    document.documentElement.setAttribute("dir", mozL10n.getDirection());
    document.body.setAttribute("platform", loop.shared.utils.getPlatform());

    // Notify the window that we've finished initalization and initial layout
    var evtObject = document.createEvent("Event");
    evtObject.initEvent("loopPanelInitialized", true, false);
    window.dispatchEvent(evtObject);
  }

  return {
    AccountLink: AccountLink,
    ConversationDropdown: ConversationDropdown,
    GettingStartedView: GettingStartedView,
    init: init,
    NewRoomView: NewRoomView,
    PanelView: PanelView,
    RoomEntry: RoomEntry,
    RoomEntryContextButtons: RoomEntryContextButtons,
    RoomList: RoomList,
    SettingsDropdown: SettingsDropdown,
    SignInRequestView: SignInRequestView,
    ToSView: ToSView
  };
})(_, document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.panel.init);
