/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.panel = function (_, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views;
  var sharedModels = loop.shared.models;
  var sharedMixins = loop.shared.mixins;
  var sharedActions = loop.shared.actions;
  var Button = sharedViews.Button;

  var FTU_VERSION = 1;

  var GettingStartedView = React.createClass({
    displayName: "GettingStartedView",

    mixins: [sharedMixins.WindowCloseMixin],

    handleButtonClick: function () {
      loop.requestMulti(["OpenGettingStartedTour", "getting-started"], ["SetLoopPref", "gettingStarted.latestFTUVersion", FTU_VERSION]).then(function () {
        var event = new CustomEvent("GettingStartedSeen");
        window.dispatchEvent(event);
      }.bind(this));
      this.closeWindow();
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "fte-get-started-content" },
        React.createElement(
          "div",
          { className: "fte-title" },
          React.createElement("img", { className: "fte-logo", src: "shared/img/hello_logo.svg" }),
          React.createElement(
            "div",
            { className: "fte-subheader" },
            mozL10n.get("first_time_experience_subheading2")
          ),
          React.createElement("hr", { className: "fte-separator" }),
          React.createElement(
            "div",
            { className: "fte-content" },
            mozL10n.get("first_time_experience_content")
          ),
          React.createElement("img", { className: "fte-hello-web-share", src: "shared/img/hello-web-share.svg" })
        ),
        React.createElement(
          "div",
          { className: "fte-button-container" },
          React.createElement(Button, { additionalClass: "fte-get-started-button",
            caption: mozL10n.get("first_time_experience_button_label2"),
            htmlId: "fte-button",
            onClick: this.handleButtonClick })
        )
      );
    }
  });

  /**
   * Displays a view requesting the user to sign-in again.
   */
  var SignInRequestView = React.createClass({
    displayName: "SignInRequestView",

    mixins: [sharedMixins.WindowCloseMixin],

    handleSignInClick: function (event) {
      event.preventDefault();
      loop.request("LoginToFxA", true);
      this.closeWindow();
    },

    handleGuestClick: function () {
      loop.request("LogoutFromFxA");
    },

    render: function () {
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

      return React.createElement(
        "div",
        { className: "sign-in-request" },
        React.createElement(
          "h1",
          null,
          line1
        ),
        React.createElement(
          "h2",
          null,
          line2
        ),
        React.createElement(
          "div",
          null,
          React.createElement(
            "button",
            { className: "btn btn-info sign-in-request-button",
              onClick: this.handleSignInClick },
            mozL10n.get("sign_in_again_button")
          )
        ),
        React.createElement(
          "a",
          { onClick: this.handleGuestClick },
          useGuestString
        )
      );
    }
  });

  var ToSView = React.createClass({
    displayName: "ToSView",

    mixins: [sharedMixins.WindowCloseMixin],

    getInitialState: function () {
      return {
        terms_of_use_url: loop.getStoredRequest(["GetLoopPref", "legal.ToS_url"]),
        privacy_notice_url: loop.getStoredRequest(["GetLoopPref", "legal.privacy_url"])
      };
    },

    handleLinkClick: function (event) {
      if (!event.target || !event.target.href) {
        return;
      }

      event.preventDefault();
      loop.request("OpenURL", event.target.href);
      this.closeWindow();
    },

    render: function () {
      var locale = mozL10n.language.code;
      var tosHTML = mozL10n.get("legal_text_and_links3", {
        "clientShortname": mozL10n.get("clientShortname2"),
        "terms_of_use": React.renderToStaticMarkup(React.createElement(
          "a",
          { href: this.state.terms_of_use_url, target: "_blank" },
          mozL10n.get("legal_text_tos")
        )),
        "privacy_notice": React.renderToStaticMarkup(React.createElement(
          "a",
          { href: this.state.privacy_notice_url, target: "_blank" },
          mozL10n.get("legal_text_privacy")
        ))
      });

      return React.createElement(
        "div",
        { className: "powered-by-wrapper", id: "powered-by-wrapper" },
        React.createElement(
          "p",
          { className: "powered-by", id: "powered-by" },
          mozL10n.get("powered_by_beforeLogo"),
          React.createElement("span", { className: locale, id: "powered-by-logo" }),
          mozL10n.get("powered_by_afterLogo")
        ),
        React.createElement("p", { className: "terms-service",
          dangerouslySetInnerHTML: { __html: tosHTML },
          onClick: this.handleLinkClick })
      );
    }
  });

  /**
   * Panel settings (gear) menu entry.
   */
  var SettingsDropdownEntry = React.createClass({
    displayName: "SettingsDropdownEntry",

    propTypes: {
      displayed: React.PropTypes.bool,
      extraCSSClass: React.PropTypes.string,
      label: React.PropTypes.string.isRequired,
      onClick: React.PropTypes.func.isRequired
    },

    getDefaultProps: function () {
      return { displayed: true };
    },

    render: function () {
      var cx = classNames;

      if (!this.props.displayed) {
        return null;
      }

      var extraCSSClass = {
        "dropdown-menu-item": true
      };
      if (this.props.extraCSSClass) {
        extraCSSClass[this.props.extraCSSClass] = true;
      }

      return React.createElement(
        "li",
        { className: cx(extraCSSClass), onClick: this.props.onClick },
        this.props.label
      );
    }
  });

  /**
   * Panel settings (gear) menu.
   */
  var SettingsDropdown = React.createClass({
    displayName: "SettingsDropdown",

    mixins: [sharedMixins.DropdownMenuMixin(), sharedMixins.WindowCloseMixin],

    getInitialState: function () {
      return {
        signedIn: !!loop.getStoredRequest(["GetUserProfile"]),
        fxAEnabled: loop.getStoredRequest(["GetFxAEnabled"]),
        doNotDisturb: loop.getStoredRequest(["GetDoNotDisturb"])
      };
    },

    componentWillUpdate: function (nextProps, nextState) {
      if (nextState.showMenu !== this.state.showMenu) {
        loop.requestMulti(["GetUserProfile"], ["GetFxAEnabled"], ["GetDoNotDisturb"]).then(function (results) {
          this.setState({
            signedIn: !!results[0],
            fxAEnabled: results[1],
            doNotDisturb: results[2]
          });
        }.bind(this));
      }
    },

    handleClickSettingsEntry: function () {
      // XXX to be implemented at the same time as unhiding the entry
    },

    handleClickAccountEntry: function () {
      loop.request("OpenFxASettings");
      this.closeWindow();
    },

    handleClickAuthEntry: function () {
      if (this.state.signedIn) {
        loop.request("LogoutFromFxA");
        // Close the menu but leave the panel open
        this.hideDropdownMenu();
      } else {
        loop.request("LoginToFxA");
        // Close the panel, the menu will be closed by on blur listener of DropdownMenuMixin
        this.closeWindow();
      }
    },

    handleHelpEntry: function (event) {
      event.preventDefault();
      loop.request("GetLoopPref", "support_url").then(function (helloSupportUrl) {
        loop.request("OpenURL", helloSupportUrl);
        this.closeWindow();
      }.bind(this));
    },

    handleToggleNotifications: function () {
      loop.request("GetDoNotDisturb").then(function (result) {
        loop.request("SetDoNotDisturb", !result);
      });
      this.hideDropdownMenu();
    },

    /**
     * Load on the browser the feedback url from prefs
     */
    handleSubmitFeedback: function (event) {
      event.preventDefault();
      loop.request("GetLoopPref", "feedback.manualFormURL").then(function (helloFeedbackUrl) {
        loop.request("OpenURL", helloFeedbackUrl);
        this.closeWindow();
      }.bind(this));
    },

    openGettingStartedTour: function () {
      loop.request("OpenGettingStartedTour", "settings-menu");
      this.closeWindow();
    },

    render: function () {
      var cx = classNames;
      var accountEntryCSSClass = this.state.signedIn ? "entry-settings-signout" : "entry-settings-signin";
      var notificationsLabel = this.state.doNotDisturb ? "settings_menu_item_turnnotificationson" : "settings_menu_item_turnnotificationsoff";

      return React.createElement(
        "div",
        { className: "settings-menu dropdown" },
        React.createElement("button", { className: "button-settings",
          onClick: this.toggleDropdownMenu,
          ref: "menu-button",
          title: mozL10n.get("settings_menu_button_tooltip") }),
        React.createElement(
          "ul",
          { className: cx({ "dropdown-menu": true, hide: !this.state.showMenu }) },
          React.createElement(SettingsDropdownEntry, {
            extraCSSClass: "entry-settings-notifications entries-divider",
            label: mozL10n.get(notificationsLabel),
            onClick: this.handleToggleNotifications }),
          React.createElement(SettingsDropdownEntry, {
            displayed: this.state.signedIn && this.state.fxAEnabled,
            extraCSSClass: "entry-settings-account",
            label: mozL10n.get("settings_menu_item_account"),
            onClick: this.handleClickAccountEntry }),
          React.createElement(SettingsDropdownEntry, { displayed: false,
            label: mozL10n.get("settings_menu_item_settings"),
            onClick: this.handleClickSettingsEntry }),
          React.createElement(SettingsDropdownEntry, { label: mozL10n.get("tour_label"),
            onClick: this.openGettingStartedTour }),
          React.createElement(SettingsDropdownEntry, { extraCSSClass: "entry-settings-feedback",
            label: mozL10n.get("settings_menu_item_feedback"),
            onClick: this.handleSubmitFeedback }),
          React.createElement(SettingsDropdownEntry, { displayed: this.state.fxAEnabled,
            extraCSSClass: accountEntryCSSClass,
            label: this.state.signedIn ? mozL10n.get("settings_menu_item_signout") : mozL10n.get("settings_menu_item_signin"),
            onClick: this.handleClickAuthEntry }),
          React.createElement(SettingsDropdownEntry, { extraCSSClass: "entry-settings-help",
            label: mozL10n.get("help_label"),
            onClick: this.handleHelpEntry })
        )
      );
    }
  });

  /**
   * FxA sign in/up link component.
   */
  var AccountLink = React.createClass({
    displayName: "AccountLink",

    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      fxAEnabled: React.PropTypes.bool.isRequired,
      userProfile: userProfileValidator
    },

    handleSignInLinkClick: function () {
      loop.request("LoginToFxA");
      this.closeWindow();
    },

    render: function () {
      if (!this.props.fxAEnabled) {
        return null;
      }

      if (this.props.userProfile && this.props.userProfile.email) {
        return React.createElement(
          "div",
          { className: "user-identity" },
          this.props.userProfile.email
        );
      }

      return React.createElement(
        "p",
        { className: "signin-link" },
        React.createElement(
          "a",
          { href: "#", onClick: this.handleSignInLinkClick },
          mozL10n.get("panel_footer_signin_or_signup_link")
        )
      );
    }
  });

  var RoomEntryContextItem = React.createClass({
    displayName: "RoomEntryContextItem",

    mixins: [loop.shared.mixins.WindowCloseMixin],

    propTypes: {
      roomUrls: React.PropTypes.array
    },

    handleClick: function (event) {
      event.stopPropagation();
      event.preventDefault();
      if (event.currentTarget.href) {
        loop.request("OpenURL", event.currentTarget.href);
        this.closeWindow();
      }
    },

    _renderDefaultIcon: function () {
      return React.createElement(
        "div",
        { className: "room-entry-context-item" },
        React.createElement("img", { src: "shared/img/icons-16x16.svg#globe" })
      );
    },

    _renderIcon: function (roomUrl) {
      return React.createElement(
        "div",
        { className: "room-entry-context-item" },
        React.createElement(
          "a",
          { href: roomUrl.location,
            onClick: this.handleClick,
            title: roomUrl.description },
          React.createElement("img", { src: roomUrl.thumbnail || "shared/img/icons-16x16.svg#globe" })
        )
      );
    },

    render: function () {
      var roomUrl = this.props.roomUrls && this.props.roomUrls[0];
      if (roomUrl && roomUrl.location) {
        return this._renderIcon(roomUrl);
      }

      return this._renderDefaultIcon();
    }
  });

  /**
   * Room list entry.
   *
   * Active Room means there are participants in the room.
   * Opened Room means the user is in the room.
   */
  var RoomEntry = React.createClass({
    displayName: "RoomEntry",

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      isOpenedRoom: React.PropTypes.bool.isRequired,
      room: React.PropTypes.instanceOf(loop.store.Room).isRequired
    },

    mixins: [loop.shared.mixins.WindowCloseMixin, sharedMixins.DropdownMenuMixin()],

    getInitialState: function () {
      return {
        eventPosY: 0
      };
    },

    _isActive: function () {
      return this.props.room.participants.length > 0;
    },

    handleClickEntry: function (event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.OpenRoom({
        roomToken: this.props.room.roomToken
      }));

      // Open url if needed.
      loop.request("getSelectedTabMetadata").then(function (metadata) {
        var contextURL = this.props.room.decryptedContext.urls && this.props.room.decryptedContext.urls[0].location;
        if (contextURL && metadata.url !== contextURL) {
          loop.request("OpenURL", contextURL);
        }
        this.closeWindow();
      }.bind(this));
    },

    handleClick: function (e) {
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
    _handleMouseOut: function () {
      if (this.state.showMenu) {
        this.toggleDropdownMenu();
      }
    },

    render: function () {
      var roomClasses = classNames({
        "room-entry": true,
        "room-active": this._isActive(),
        "room-opened": this.props.isOpenedRoom
      });
      var urlData = (this.props.room.decryptedContext.urls || [])[0] || {};
      var roomTitle = this.props.room.decryptedContext.roomName || urlData.description || urlData.location || mozL10n.get("room_name_untitled_page");

      return React.createElement(
        "div",
        { className: roomClasses,
          onClick: this.props.isOpenedRoom ? null : this.handleClickEntry,
          onMouseLeave: this.props.isOpenedRoom ? null : this._handleMouseOut,
          ref: "roomEntry" },
        React.createElement(RoomEntryContextItem, {
          roomUrls: this.props.room.decryptedContext.urls }),
        React.createElement(
          "h2",
          null,
          roomTitle
        ),
        this.props.isOpenedRoom ? null : React.createElement(RoomEntryContextButtons, {
          dispatcher: this.props.dispatcher,
          eventPosY: this.state.eventPosY,
          handleClick: this.handleClick,
          ref: "contextActions",
          room: this.props.room,
          showMenu: this.state.showMenu,
          toggleDropdownMenu: this.toggleDropdownMenu })
      );
    }
  });

  /**
   * Buttons corresponding to each conversation entry.
   * This component renders the edit button for displaying contextual dropdown
   * menu for conversation entries. It also holds the dropdown menu.
   */
  var RoomEntryContextButtons = React.createClass({
    displayName: "RoomEntryContextButtons",

    propTypes: {
      dispatcher: React.PropTypes.object.isRequired,
      eventPosY: React.PropTypes.number.isRequired,
      handleClick: React.PropTypes.func.isRequired,
      room: React.PropTypes.object.isRequired,
      showMenu: React.PropTypes.bool.isRequired,
      toggleDropdownMenu: React.PropTypes.func.isRequired
    },

    handleEmailButtonClick: function (event) {
      event.preventDefault();
      event.stopPropagation();

      this.props.dispatcher.dispatch(new sharedActions.EmailRoomUrl({
        roomUrl: this.props.room.roomUrl,
        from: "panel"
      }));

      this.props.toggleDropdownMenu();
    },

    handleCopyButtonClick: function (event) {
      event.stopPropagation();
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.CopyRoomUrl({
        roomUrl: this.props.room.roomUrl,
        from: "panel"
      }));

      this.props.toggleDropdownMenu();
    },

    handleDeleteButtonClick: function (event) {
      event.stopPropagation();
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.DeleteRoom({
        roomToken: this.props.room.roomToken
      }));

      this.props.toggleDropdownMenu();
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "room-entry-context-actions" },
        React.createElement("div", {
          className: "room-entry-context-edit-btn dropdown-menu-button",
          onClick: this.props.handleClick,
          ref: "menu-button" }),
        this.props.showMenu ? React.createElement(ConversationDropdown, {
          eventPosY: this.props.eventPosY,
          handleCopyButtonClick: this.handleCopyButtonClick,
          handleDeleteButtonClick: this.handleDeleteButtonClick,
          handleEmailButtonClick: this.handleEmailButtonClick,
          ref: "menu" }) : null
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
    displayName: "ConversationDropdown",

    propTypes: {
      eventPosY: React.PropTypes.number.isRequired,
      handleCopyButtonClick: React.PropTypes.func.isRequired,
      handleDeleteButtonClick: React.PropTypes.func.isRequired,
      handleEmailButtonClick: React.PropTypes.func.isRequired
    },

    getInitialState: function () {
      return {
        openDirUp: false
      };
    },

    componentDidMount: function () {
      var menuNode = this.getDOMNode();
      var menuNodeRect = menuNode.getBoundingClientRect();

      // Get the parent element and make sure the menu does not overlow its
      // container.
      var listNode = loop.shared.utils.findParentNode(this.getDOMNode(), "rooms");
      var listNodeRect = listNode.getBoundingClientRect();

      // Click offset to not display the menu right next to the area clicked.
      var offset = 10;

      if (this.props.eventPosY + menuNodeRect.height >= listNodeRect.top + listNodeRect.height) {
        // Position above click area.
        menuNode.style.top = this.props.eventPosY - menuNodeRect.height - listNodeRect.top - offset + "px";
      } else {
        // Position below click area.
        menuNode.style.top = this.props.eventPosY - listNodeRect.top + offset + "px";
      }
    },

    render: function () {
      var dropdownClasses = classNames({
        "dropdown-menu": true,
        "dropdown-menu-up": this.state.openDirUp
      });

      return React.createElement(
        "ul",
        { className: dropdownClasses },
        React.createElement(
          "li",
          {
            className: "dropdown-menu-item",
            onClick: this.props.handleCopyButtonClick,
            ref: "copyButton" },
          mozL10n.get("copy_link_menuitem")
        ),
        React.createElement(
          "li",
          {
            className: "dropdown-menu-item",
            onClick: this.props.handleEmailButtonClick,
            ref: "emailButton" },
          mozL10n.get("email_link_menuitem")
        ),
        React.createElement(
          "li",
          {
            className: "dropdown-menu-item",
            onClick: this.props.handleDeleteButtonClick,
            ref: "deleteButton" },
          mozL10n.get("delete_conversation_menuitem2")
        )
      );
    }
  });

  /**
   * User profile prop can be either an object or null as per mozLoopAPI
   * and there is no way to express this with React 0.13.3
   */
  function userProfileValidator(props, propName, componentName) {
    if (Object.prototype.toString.call(props[propName]) !== "[object Object]" && !_.isNull(props[propName])) {
      return new Error("Required prop `" + propName + "` was not correctly specified in `" + componentName + "`.");
    }
  }

  /**
   * Room list.
   */
  var RoomList = React.createClass({
    displayName: "RoomList",

    mixins: [Backbone.Events, sharedMixins.WindowCloseMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      store: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    getInitialState: function () {
      return this.props.store.getStoreState();
    },

    componentDidMount: function () {
      this.listenTo(this.props.store, "change", this._onStoreStateChanged);

      // XXX this should no longer be necessary once have a better mechanism
      // for updating the list (possibly as part of the content side of bug
      // 1074665.
      this.props.dispatcher.dispatch(new sharedActions.GetAllRooms());
    },

    componentWillUnmount: function () {
      this.stopListening(this.props.store);
    },

    componentWillUpdate: function (nextProps, nextState) {
      // If we've just created a room, close the panel - the store will open
      // the room.
      if (this.state.pendingCreation && !nextState.pendingCreation && !nextState.error) {
        this.closeWindow();
      }
    },

    _onStoreStateChanged: function () {
      this.setState(this.props.store.getStoreState());
    },

    /**
     * Let the user know we're loading rooms
     * @returns {Object} React render
     */
    _renderLoadingRoomsView: function () {
      /* XXX should refactor and separate "rooms" amd perhaps room-list so that
      we arent duplicating those elements all over */
      return React.createElement(
        "div",
        { className: "rooms" },
        this._renderNewRoomButton(),
        React.createElement(
          "div",
          { className: "room-list" },
          React.createElement(
            "div",
            { className: "room-list-loading" },
            React.createElement("img", { src: "shared/img/animated-spinner.svg" })
          )
        )
      );
    },

    _renderNewRoomButton: function () {
      return React.createElement(NewRoomView, { dispatcher: this.props.dispatcher,
        inRoom: this.state.openedRoom !== null,
        pendingOperation: this.state.pendingCreation || this.state.pendingInitialRetrieval });
    },

    _addListGradientIfNeeded: function () {
      if (this.state.rooms.length > 5) {
        return React.createElement("div", { className: "room-list-blur" });
      }
    },

    render: function () {
      var roomListClasses = classNames({
        "room-list": true,
        // add extra space to last item so when scrolling to bottom,
        // last item is not covered by the gradient
        "room-list-add-space": this.state.rooms.length && this.state.rooms.length > 5
      });

      if (this.state.error) {
        // XXX Better end user reporting of errors.
        console.error("RoomList error", this.state.error);
      }

      if (this.state.pendingInitialRetrieval) {
        return this._renderLoadingRoomsView();
      }

      return React.createElement(
        "div",
        { className: "rooms" },
        this._renderNewRoomButton(),
        !this.state.rooms.length ? null : React.createElement(
          "h1",
          null,
          mozL10n.get(this.state.openedRoom === null ? "rooms_list_recently_browsed2" : "rooms_list_currently_browsing2")
        ),
        !this.state.rooms.length ? React.createElement("div", { className: "room-list-empty" }) : React.createElement(
          "div",
          { className: roomListClasses },
          this.state.rooms.map(function (room) {
            if (this.state.openedRoom !== null && room.roomToken !== this.state.openedRoom) {
              return null;
            }

            return React.createElement(RoomEntry, {
              dispatcher: this.props.dispatcher,
              isOpenedRoom: room.roomToken === this.state.openedRoom,
              key: room.roomToken,
              room: room });
          }, this)
        ),
        this._addListGradientIfNeeded()
      );
    }
  });

  /**
   * Used for creating a new room with or without context.
   */
  var NewRoomView = React.createClass({
    displayName: "NewRoomView",

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      inRoom: React.PropTypes.bool.isRequired,
      pendingOperation: React.PropTypes.bool.isRequired
    },

    mixins: [sharedMixins.DocumentVisibilityMixin, React.addons.PureRenderMixin],

    getInitialState: function () {
      return {
        previewImage: "",
        description: "",
        url: ""
      };
    },

    onDocumentVisible: function () {
      // We would use onDocumentHidden to null out the data ready for the next
      // opening. However, this seems to cause an awkward glitch in the display
      // when opening the panel, and it seems cleaner just to update the data
      // even if there's a small delay.

      loop.request("GetSelectedTabMetadata").then(function (metadata) {
        // Bail out when the component is not mounted (anymore).
        // This occurs during test runs. See bug 1174611 for more info.
        if (!this.isMounted() || !metadata) {
          return;
        }

        var previewImage = metadata.favicon || "";
        var description = metadata.title || metadata.description;
        var url = metadata.url;
        this.setState({
          previewImage: previewImage,
          description: description,
          url: url
        });
      }.bind(this));
    },

    handleCreateButtonClick: function () {
      var createRoomAction = new sharedActions.CreateRoom();

      createRoomAction.urls = [{
        location: this.state.url,
        description: this.state.description,
        thumbnail: this.state.previewImage
      }];
      this.props.dispatcher.dispatch(createRoomAction);
    },

    handleStopSharingButtonClick: function () {
      loop.request("HangupAllChatWindows");
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "new-room-view" },
        this.props.inRoom ? React.createElement(
          "button",
          { className: "btn btn-info stop-sharing-button",
            disabled: this.props.pendingOperation,
            onClick: this.handleStopSharingButtonClick },
          mozL10n.get("panel_disconnect_button")
        ) : React.createElement(
          "button",
          { className: "btn btn-info new-room-button",
            disabled: this.props.pendingOperation,
            onClick: this.handleCreateButtonClick },
          mozL10n.get("panel_browse_with_friend_button")
        )
      );
    }
  });

  /**
   * E10s not supported view
   */
  var E10sNotSupported = React.createClass({
    displayName: "E10sNotSupported",

    propTypes: {
      onClick: React.PropTypes.func.isRequired
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "error-content" },
        React.createElement(
          "header",
          { className: "error-title" },
          React.createElement("img", { src: "shared/img/sad_hello_icon_64x64.svg" }),
          React.createElement(
            "p",
            { className: "error-subheader" },
            mozL10n.get("e10s_not_supported_subheading", {
              brandShortname: mozL10n.get("clientShortname2")
            })
          )
        ),
        React.createElement(Button, { additionalClass: "e10s-not-supported-button",
          caption: mozL10n.get("e10s_not_supported_button_label"),
          onClick: this.props.onClick })
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({
    displayName: "PanelView",

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      // Only used for the ui-showcase:
      gettingStartedSeen: React.PropTypes.bool,
      notifications: React.PropTypes.object.isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      // Only used for the ui-showcase:
      userProfile: React.PropTypes.object
    },

    getDefaultProps: function () {
      return {
        gettingStartedSeen: true
      };
    },

    getInitialState: function () {
      return {
        fxAEnabled: loop.getStoredRequest(["GetFxAEnabled"]),
        hasEncryptionKey: loop.getStoredRequest(["GetHasEncryptionKey"]),
        userProfile: loop.getStoredRequest(["GetUserProfile"]),
        gettingStartedSeen: loop.getStoredRequest(["GetLoopPref", "gettingStarted.latestFTUVersion"]) >= FTU_VERSION,
        multiProcessEnabled: loop.getStoredRequest(["IsMultiProcessEnabled"])
      };
    },

    _serviceErrorToShow: function () {
      return new Promise(function (resolve) {
        loop.request("GetErrors").then(function (errors) {
          if (!errors || !Object.keys(errors).length) {
            resolve(null);
            return;
          }
          // Just get the first error for now since more than one should be rare.
          var firstErrorKey = Object.keys(errors)[0];
          resolve({
            type: firstErrorKey,
            error: errors[firstErrorKey]
          });
        });
      });
    },

    updateServiceErrors: function () {
      this._serviceErrorToShow().then(function (serviceError) {
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
      }.bind(this));
    },

    _onStatusChanged: function () {
      loop.requestMulti(["GetUserProfile"], ["GetHasEncryptionKey"]).then(function (results) {
        var profile = results[0];
        var hasEncryptionKey = results[1];
        var currUid = this.state.userProfile ? this.state.userProfile.uid : null;
        var newUid = profile ? profile.uid : null;
        if (currUid === newUid) {
          // Update the state of hasEncryptionKey as this might have changed now.
          this.setState({ hasEncryptionKey: hasEncryptionKey });
        } else {
          this.setState({ userProfile: profile });
        }
        this.updateServiceErrors();
      }.bind(this));
    },

    _gettingStartedSeen: function () {
      loop.request("GetLoopPref", "gettingStarted.latestFTUVersion").then(function (result) {
        this.setState({
          gettingStartedSeen: result >= FTU_VERSION
        });
      }.bind(this));
    },

    componentWillMount: function () {
      this.updateServiceErrors();
    },

    componentDidMount: function () {
      loop.subscribe("LoopStatusChanged", this._onStatusChanged);
      window.addEventListener("GettingStartedSeen", this._gettingStartedSeen);
    },

    componentWillUnmount: function () {
      loop.unsubscribe("LoopStatusChanged", this._onStatusChanged);
      window.removeEventListener("GettingStartedSeen", this._gettingStartedSeen);
    },

    handleContextMenu: function (e) {
      e.preventDefault();
    },

    launchNonE10sWindow: function () {
      loop.request("GetSelectedTabMetadata").then(function (metadata) {
        loop.request("OpenNonE10sWindow", metadata.url);
      });
    },

    render: function () {
      var NotificationListView = sharedViews.NotificationListView;

      if (this.state.multiProcessEnabled) {
        return React.createElement(E10sNotSupported, { onClick: this.launchNonE10sWindow });
      }

      if (!this.props.gettingStartedSeen || !this.state.gettingStartedSeen) {
        return React.createElement(
          "div",
          { className: "fte-get-started-container",
            onContextMenu: this.handleContextMenu },
          React.createElement(NotificationListView, {
            clearOnDocumentHidden: true,
            notifications: this.props.notifications }),
          React.createElement(GettingStartedView, null),
          React.createElement(ToSView, null)
        );
      }
      if (!this.state.hasEncryptionKey) {
        return React.createElement(SignInRequestView, null);
      }

      return React.createElement(
        "div",
        { className: "panel-content",
          onContextMenu: this.handleContextMenu },
        React.createElement("div", { className: "beta-ribbon" }),
        React.createElement(NotificationListView, {
          clearOnDocumentHidden: true,
          notifications: this.props.notifications }),
        React.createElement(RoomList, { dispatcher: this.props.dispatcher,
          store: this.props.roomStore }),
        React.createElement(
          "div",
          { className: "footer" },
          React.createElement(AccountLink, { fxAEnabled: this.state.fxAEnabled,
            userProfile: this.props.userProfile || this.state.userProfile }),
          React.createElement(
            "div",
            { className: "signin-details" },
            React.createElement(SettingsDropdown, null)
          )
        )
      );
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    var requests = [["GetAllConstants"], ["GetAllStrings"], ["GetLocale"], ["GetPluralRule"]];
    var prefetch = [["GetLoopPref", "gettingStarted.latestFTUVersion"], ["GetLoopPref", "legal.ToS_url"], ["GetLoopPref", "legal.privacy_url"], ["GetUserProfile"], ["GetFxAEnabled"], ["GetDoNotDisturb"], ["GetHasEncryptionKey"], ["IsMultiProcessEnabled"]];

    return loop.requestMulti.apply(null, requests.concat(prefetch)).then(function (results) {
      // `requestIdx` is keyed off the order of the `requests` and `prefetch`
      // arrays. Be careful to update both when making changes.
      var requestIdx = 0;
      var constants = results[requestIdx];
      // Do the initial L10n setup, we do this before anything
      // else to ensure the L10n environment is setup correctly.
      var stringBundle = results[++requestIdx];
      var locale = results[++requestIdx];
      var pluralRule = results[++requestIdx];
      mozL10n.initialize({
        locale: locale,
        pluralRule: pluralRule,
        getStrings: function (key) {
          if (!(key in stringBundle)) {
            console.error("No string found for key: ", key);
            return "{ textContent: '' }";
          }

          return JSON.stringify({ textContent: stringBundle[key] });
        }
      });

      prefetch.forEach(function (req) {
        req.shift();
        loop.storeRequest(req, results[++requestIdx]);
      });

      var notifications = new sharedModels.NotificationCollection();
      var dispatcher = new loop.Dispatcher();
      var roomStore = new loop.store.RoomStore(dispatcher, {
        notifications: notifications,
        constants: constants
      });

      React.render(React.createElement(PanelView, {
        dispatcher: dispatcher,
        notifications: notifications,
        roomStore: roomStore }), document.querySelector("#main"));

      document.documentElement.setAttribute("lang", mozL10n.language.code);
      document.documentElement.setAttribute("dir", mozL10n.language.direction);
      document.body.setAttribute("platform", loop.shared.utils.getPlatform());

      // Notify the window that we've finished initalization and initial layout
      var evtObject = document.createEvent("Event");
      evtObject.initEvent("loopPanelInitialized", true, false);
      window.dispatchEvent(evtObject);
    });
  }

  return {
    AccountLink: AccountLink,
    ConversationDropdown: ConversationDropdown,
    E10sNotSupported: E10sNotSupported,
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
}(_, document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.panel.init);
