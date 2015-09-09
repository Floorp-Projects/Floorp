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
  var sharedUtils = loop.shared.utils;
  var Button = sharedViews.Button;
  var ButtonGroup = sharedViews.ButtonGroup;
  var Checkbox = sharedViews.Checkbox;
  var ContactsControllerView = loop.contacts.ContactsControllerView;

  var TabView = React.createClass({
    propTypes: {
      buttonsHidden: React.PropTypes.array,
      children: React.PropTypes.arrayOf(React.PropTypes.element),
      mozLoop: React.PropTypes.object,
      // The selectedTab prop is used by the UI showcase.
      selectedTab: React.PropTypes.string
    },

    getDefaultProps: function() {
      return {
        buttonsHidden: []
      };
    },

    shouldComponentUpdate: function(nextProps, nextState) {
      var tabChange = this.state.selectedTab !== nextState.selectedTab;
      if (tabChange) {
        this.props.mozLoop.notifyUITour("Loop:PanelTabChanged", nextState.selectedTab);
      }

      if (!tabChange && nextProps.buttonsHidden) {
        if (nextProps.buttonsHidden.length !== this.props.buttonsHidden.length) {
          tabChange = true;
        } else {
          for (var i = 0, l = nextProps.buttonsHidden.length; i < l && !tabChange; ++i) {
            if (this.props.buttonsHidden.indexOf(nextProps.buttonsHidden[i]) === -1) {
              tabChange = true;
            }
          }
        }
      }
      return tabChange;
    },

    getInitialState: function() {
      // XXX Work around props.selectedTab being undefined initially.
      // When we don't need to rely on the pref, this can move back to
      // getDefaultProps (bug 1100258).
      return {
        selectedTab: this.props.selectedTab || "rooms"
      };
    },

    handleSelectTab: function(event) {
      var tabName = event.target.dataset.tabName;
      this.setState({selectedTab: tabName});
    },

    render: function() {
      var cx = React.addons.classSet;
      var tabButtons = [];
      var tabs = [];
      React.Children.forEach(this.props.children, function(tab, i) {
        // Filter out null tabs (eg. rooms when the feature is disabled)
        if (!tab) {
          return;
        }
        var tabName = tab.props.name;
        if (this.props.buttonsHidden.indexOf(tabName) > -1) {
          return;
        }
        var isSelected = (this.state.selectedTab === tabName);
        if (!tab.props.hidden) {
          var label = mozL10n.get(tabName + "_tab_button");
          tabButtons.push(
            <li className={cx({selected: isSelected})}
                data-tab-name={tabName}
                key={i}
                onClick={this.handleSelectTab}>
              <div>{label}</div>
            </li>
          );
        }
        tabs.push(
          <div className={cx({tab: true, selected: isSelected})} key={i}>
            {tab.props.children}
          </div>
        );
      }, this);
      return (
        <div className="tab-view-container">
          <ul className="tab-view">
            {tabButtons}
            <li className="slide-bar" />
          </ul>
          {tabs}
        </div>
      );
    }
  });

  var Tab = React.createClass({
    render: function() {
      return null;
    }
  });

  /**
   * Availability drop down menu subview.
   */
  var AvailabilityDropdown = React.createClass({
    mixins: [sharedMixins.DropdownMenuMixin()],

    getInitialState: function() {
      return {
        doNotDisturb: navigator.mozLoop.doNotDisturb
      };
    },

    // XXX target event can either be the li, the span or the i tag
    // this makes it easier to figure out the target by making a
    // closure with the desired status already passed in.
    changeAvailability: function(newAvailabilty) {
      return function(event) {
        // Note: side effect!
        switch (newAvailabilty) {
          case "available":
            this.setState({doNotDisturb: false});
            navigator.mozLoop.doNotDisturb = false;
            break;
          case "do-not-disturb":
            this.setState({doNotDisturb: true});
            navigator.mozLoop.doNotDisturb = true;
            break;
        }
        this.hideDropdownMenu();
      }.bind(this);
    },

    render: function() {
      var cx = React.addons.classSet;
      var availabilityDropdown = cx({
        "dropdown-menu": true,
        "hide": !this.state.showMenu
      });
      var statusIcon = cx({
        "status-unavailable": this.state.doNotDisturb,
        "status-available": !this.state.doNotDisturb
      });
      var availabilityText = this.state.doNotDisturb ?
                             mozL10n.get("display_name_dnd_status") :
                             mozL10n.get("display_name_available_status");

      return (
        <div className="dropdown">
          <p className="dnd-status">
            <span className={statusIcon}
                  onClick={this.toggleDropdownMenu}
                  ref="menu-button">
              {availabilityText}
            </span>
          </p>
          <ul className={availabilityDropdown}>
            <li className="dropdown-menu-item status-available"
                onClick={this.changeAvailability("available")}>
              <span>{mozL10n.get("display_name_available_status")}</span>
            </li>
            <li className="dropdown-menu-item status-unavailable"
                onClick={this.changeAvailability("do-not-disturb")}>
              <span>{mozL10n.get("display_name_dnd_status")}</span>
            </li>
          </ul>
        </div>
      );
    }
  });

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
             dangerouslySetInnerHTML={{__html: tosHTML}}
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
      return {displayed: true};
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

      return (
        <div className="settings-menu dropdown">
          <button className="button-settings"
             onClick={this.toggleDropdownMenu}
             ref="menu-button"
             title={mozL10n.get("settings_menu_button_tooltip")} />
          <ul className={cx({"dropdown-menu": true, hide: !this.state.showMenu})}>
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
      this.props.mozLoop.openURL(event.currentTarget.href);
      this.closeWindow();
    },

    render: function() {
      var roomUrl = this.props.roomUrls && this.props.roomUrls[0];
      if (!roomUrl) {
        return null;
      }

      return (
        <div className="room-entry-context-item">
          <a href={roomUrl.location} onClick={this.handleClick} title={roomUrl.description}>
            <img src={roomUrl.thumbnail || "loop/shared/img/icons-16x16.svg#globe"} />
          </a>
        </div>
      );
    }
  });

  /**
   * Room list entry.
   */
  var RoomEntry = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
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

    handleContextChevronClick: function(e) {
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
        "room-active": this._isActive()
      });

      return (
        <div className={roomClasses}
          onClick={this.handleClickEntry}
          onMouseLeave={this._handleMouseOut}
          ref="roomEntry">
          <h2>
            {this.props.room.decryptedContext.roomName}
          </h2>
          <RoomEntryContextItem
            mozLoop={this.props.mozLoop}
            roomUrls={this.props.room.decryptedContext.urls} />
          <RoomEntryContextButtons
            dispatcher={this.props.dispatcher}
            eventPosY={this.state.eventPosY}
            handleClickEntry={this.handleClickEntry}
            handleContextChevronClick={this.handleContextChevronClick}
            ref="contextActions"
            room={this.props.room}
            showMenu={this.state.showMenu}
            toggleDropdownMenu={this.toggleDropdownMenu} />
        </div>
      );
    }
  });

  /**
   * Buttons corresponding to each conversation entry.
   * This component renders the video icon call button and chevron button for
   * displaying contextual dropdown menu for conversation entries.
   * It also holds the dropdown menu.
   */
  var RoomEntryContextButtons = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.object.isRequired,
      eventPosY: React.PropTypes.number.isRequired,
      handleClickEntry: React.PropTypes.func.isRequired,
      handleContextChevronClick: React.PropTypes.func.isRequired,
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
          <button
            className="btn room-entry-call-btn"
            onClick={this.props.handleClickEntry}
            ref="callButton" />
          <div
            className="room-entry-context-menu-chevron dropdown-menu-button"
            onClick={this.props.handleContextChevronClick}
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
                                                      ".rooms");
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
            {mozL10n.get("copy_url_button2")}
          </li>
          <li
            className="dropdown-menu-item"
            onClick={this.props.handleEmailButtonClick}
            ref="emailButton">
            {mozL10n.get("email_link_button")}
          </li>
          <li
            className="dropdown-menu-item"
            onClick={this.props.handleDeleteButtonClick}
            ref="deleteButton">
            {mozL10n.get("rooms_list_delete_tooltip")}
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
      store: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      // Used for room creation, associated with room owner.
      userProfile: userProfileValidator
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

    _getUserDisplayName: function() {
      return this.props.userProfile && this.props.userProfile.email ||
        mozL10n.get("display_name_guest");
    },

    /**
     * Let the user know we're loading rooms
     * @returns {Object} React render
     */
    _renderLoadingRoomsView: function() {
      return (
        <div className="room-list">
          <div className="room-list-loading">
            <img src="loop/shared/img/animated-spinner.svg" />
          </div>
          {this._renderNewRoomButton()}
        </div>
      );
    },

    _renderNoRoomsView: function() {
      return (
        <div className="rooms">
          <div className="room-list-empty">
            <div className="no-conversations-message">
              <p className="panel-text-large">
                {mozL10n.get("no_conversations_message_heading")}
              </p>
              <p className="panel-text-medium">
                {mozL10n.get("no_conversations_start_message")}
              </p>
            </div>
          </div>
          {this._renderNewRoomButton()}
        </div>
      );
    },

    _renderNewRoomButton: function() {
      return (
        <NewRoomView dispatcher={this.props.dispatcher}
          mozLoop={this.props.mozLoop}
          pendingOperation={this.state.pendingCreation ||
                            this.state.pendingInitialRetrieval}
          userDisplayName={this._getUserDisplayName()} />
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
          <h1>{mozL10n.get("rooms_list_recent_conversations")}</h1>
          <div className="room-list">{
            this.state.rooms.map(function(room, i) {
              return (
                <RoomEntry
                  dispatcher={this.props.dispatcher}
                  key={room.roomToken}
                  mozLoop={this.props.mozLoop}
                  room={room} />
              );
            }, this)
          }</div>
          {this._renderNewRoomButton()}
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
      mozLoop: React.PropTypes.object.isRequired,
      pendingOperation: React.PropTypes.bool.isRequired,
      userDisplayName: React.PropTypes.string.isRequired
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
      this.setState({checked: newState.checked});
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

    render: function() {
      var hostname;

      try {
        hostname = new URL(this.state.url).hostname;
      } catch (ex) {
        // Empty catch - if there's an error, then we won't show the context.
      }

      var contextClasses = React.addons.classSet({
        context: true,
        "context-checkbox-checked": this.state.checked,
        hide: !hostname ||
          !this.props.mozLoop.getLoopPref("contextInConversations.enabled")
      });

      return (
        <div className="new-room-view">
          <div className={contextClasses}>
            <Checkbox checked={this.state.checked}
                      label={mozL10n.get("context_inroom_label")}
                      onChange={this.onCheckboxChange} />
            <sharedViews.ContextUrlView
              allowClick={false}
              description={this.state.description}
              showContextTitle={false}
              thumbnail={this.state.previewImage}
              url={this.state.url}
              useDesktopPaths={true} />
          </div>
          <button className="btn btn-info new-room-button"
                  disabled={this.props.pendingOperation}
                  onClick={this.handleCreateButtonClick}>
            {mozL10n.get("rooms_new_room_button_label")}
          </button>
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
      initialSelectedTabComponent: React.PropTypes.string,
      mozLoop: React.PropTypes.object.isRequired,
      notifications: React.PropTypes.object.isRequired,
      roomStore:
        React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      selectedTab: React.PropTypes.string,
      // Used only for unit tests.
      showTabButtons: React.PropTypes.bool
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
        this.setState({hasEncryptionKey: this.props.mozLoop.hasEncryptionKey});
      } else {
        // On profile change (login, logout), switch back to the default tab.
        this.selectTab("rooms");
        this.setState({userProfile: profile});
      }
      this.updateServiceErrors();
    },

    _gettingStartedSeen: function() {
      this.setState({
        gettingStartedSeen: this.props.mozLoop.getLoopPref("gettingStarted.seen")
      });
    },

    _UIActionHandler: function(e) {
      switch (e.detail.action) {
        case "selectTab":
          this.selectTab(e.detail.tab);
          break;
        default:
          console.error("Invalid action", e.detail.action);
          break;
      }
    },

    selectTab: function(name) {
      // The tab view might not be created yet (e.g. getting started or fxa
      // re-sign in.
      if (this.refs.tabView) {
        this.refs.tabView.setState({ selectedTab: name });
      }
    },

    componentWillMount: function() {
      this.updateServiceErrors();
    },

    componentDidMount: function() {
      window.addEventListener("LoopStatusChanged", this._onStatusChanged);
      window.addEventListener("GettingStartedSeen", this._gettingStartedSeen);
      window.addEventListener("UIAction", this._UIActionHandler);
    },

    componentWillUnmount: function() {
      window.removeEventListener("LoopStatusChanged", this._onStatusChanged);
      window.removeEventListener("GettingStartedSeen", this._gettingStartedSeen);
      window.removeEventListener("UIAction", this._UIActionHandler);
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

      // Determine which buttons to NOT show.
      var hideButtons = [];
      if (!this.state.userProfile && !this.props.showTabButtons) {
        hideButtons.push("contacts");
      }

      return (
        <div className="panel-content">
          <NotificationListView
            clearOnDocumentHidden={true}
            notifications={this.props.notifications} />
          <TabView
            buttonsHidden={hideButtons}
            mozLoop={this.props.mozLoop}
            ref="tabView"
            selectedTab={this.props.selectedTab}>
            <Tab name="rooms">
              <RoomList dispatcher={this.props.dispatcher}
                        mozLoop={this.props.mozLoop}
                        store={this.props.roomStore}
                        userProfile={this.state.userProfile} />
            </Tab>
            <Tab name="contacts">
              <ContactsControllerView initialSelectedTabComponent={this.props.initialSelectedTabComponent}
                                      mozLoop={this.props.mozLoop}
                                      notifications={this.props.notifications}
                                      ref="contactControllerView" />
            </Tab>
          </TabView>
          <div className="footer">
            <div className="user-details">
              <AvailabilityDropdown />
            </div>
            <div className="signin-details">
              <AccountLink fxAEnabled={this.props.mozLoop.fxAEnabled}
                           userProfile={this.state.userProfile}/>
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
    AvailabilityDropdown: AvailabilityDropdown,
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
