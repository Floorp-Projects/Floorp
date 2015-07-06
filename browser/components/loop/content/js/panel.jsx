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
  var ContactsList = loop.contacts.ContactsList;
  var ContactDetailsForm = loop.contacts.ContactDetailsForm;

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
        var isSelected = (this.state.selectedTab == tabName);
        if (!tab.props.hidden) {
          tabButtons.push(
            <li className={cx({selected: isSelected})}
                data-tab-name={tabName}
                key={i}
                onClick={this.handleSelectTab}
                title={mozL10n.get(tabName + "_tab_button_tooltip")} />
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
          <ul className="tab-view">{tabButtons}</ul>
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
      // XXX https://github.com/facebook/react/issues/310 for === htmlFor
      var cx = React.addons.classSet;
      var availabilityStatus = cx({
        "status": true,
        "status-dnd": this.state.doNotDisturb,
        "status-available": !this.state.doNotDisturb
      });
      var availabilityDropdown = cx({
        "dropdown-menu": true,
        "hide": !this.state.showMenu
      });
      var availabilityText = this.state.doNotDisturb ?
                              mozL10n.get("display_name_dnd_status") :
                              mozL10n.get("display_name_available_status");

      return (
        <div className="dropdown">
          <p className="dnd-status" onClick={this.toggleDropdownMenu} ref="menu-button">
            <span>{availabilityText}</span>
            <i className={availabilityStatus}></i>
          </p>
          <ul className={availabilityDropdown}>
            <li className="dropdown-menu-item dnd-make-available"
                onClick={this.changeAvailability("available")}>
              <i className="status status-available"></i>
              <span>{mozL10n.get("display_name_available_status")}</span>
            </li>
            <li className="dropdown-menu-item dnd-make-unavailable"
                onClick={this.changeAvailability("do-not-disturb")}>
              <i className="status status-dnd"></i>
              <span>{mozL10n.get("display_name_dnd_status")}</span>
            </li>
          </ul>
        </div>
      );
    }
  });

  var GettingStartedView = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    handleButtonClick: function() {
      navigator.mozLoop.openGettingStartedTour("getting-started");
      navigator.mozLoop.setLoopPref("gettingStarted.seen", true);
      var event = new CustomEvent("GettingStartedSeen");
      window.dispatchEvent(event);
      this.closeWindow();
    },

    render: function() {
      if (navigator.mozLoop.getLoopPref("gettingStarted.seen")) {
        return null;
      }
      return (
        <div id="fte-getstarted">
          <header id="fte-title">
            {mozL10n.get("first_time_experience_title", {
              "clientShortname": mozL10n.get("clientShortname2")
            })}
          </header>
          <Button caption={mozL10n.get("first_time_experience_button_label")}
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

    getInitialState: function() {
      var getPref = navigator.mozLoop.getLoopPref.bind(navigator.mozLoop);

      return {
        seenToS: getPref("seenToS"),
        gettingStartedSeen: getPref("gettingStarted.seen"),
        showPartnerLogo: getPref("showPartnerLogo")
      };
    },

    handleLinkClick: function(event) {
      if (!event.target || !event.target.href) {
        return;
      }

      event.preventDefault();
      navigator.mozLoop.openURL(event.target.href);
      this.closeWindow();
    },

    renderPartnerLogo: function() {
      if (!this.state.showPartnerLogo) {
        return null;
      }

      var locale = mozL10n.getLanguage();
      navigator.mozLoop.setLoopPref("showPartnerLogo", false);
      return (
        <p className="powered-by" id="powered-by">
          {mozL10n.get("powered_by_beforeLogo")}
          <img className={locale} id="powered-by-logo" />
          {mozL10n.get("powered_by_afterLogo")}
        </p>
      );
    },

    render: function() {
      if (!this.state.gettingStartedSeen || this.state.seenToS == "unseen") {
        var terms_of_use_url = navigator.mozLoop.getLoopPref("legal.ToS_url");
        var privacy_notice_url = navigator.mozLoop.getLoopPref("legal.privacy_url");
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
          <div id="powered-by-wrapper">
            {this.renderPartnerLogo()}
            <p className="terms-service"
               dangerouslySetInnerHTML={{__html: tosHTML}}
               onClick={this.handleLinkClick}></p>
           </div>
        );
      } else {
        return <div />;
      }
    }
  });

  /**
   * Panel settings (gear) menu entry.
   */
  var SettingsDropdownEntry = React.createClass({
    propTypes: {
      displayed: React.PropTypes.bool,
      icon: React.PropTypes.string,
      label: React.PropTypes.string.isRequired,
      onClick: React.PropTypes.func.isRequired
    },

    getDefaultProps: function() {
      return {displayed: true};
    },

    render: function() {
      if (!this.props.displayed) {
        return null;
      }
      return (
        <li className="dropdown-menu-item" onClick={this.props.onClick}>
          {this.props.icon ?
            <i className={"icon icon-" + this.props.icon}></i> :
            null}
          <span>{this.props.label}</span>
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

      return (
        <div className="settings-menu dropdown">
          <a className="button-settings"
             onClick={this.toggleDropdownMenu}
             ref="menu-button"
             title={mozL10n.get("settings_menu_button_tooltip")} />
          <ul className={cx({"dropdown-menu": true, hide: !this.state.showMenu})}>
            <SettingsDropdownEntry displayed={false}
                                   icon="settings"
                                   label={mozL10n.get("settings_menu_item_settings")}
                                   onClick={this.handleClickSettingsEntry} />
            <SettingsDropdownEntry displayed={this._isSignedIn() && this.props.mozLoop.fxAEnabled}
                                   icon="account"
                                   label={mozL10n.get("settings_menu_item_account")}
                                   onClick={this.handleClickAccountEntry} />
            <SettingsDropdownEntry icon="tour"
                                   label={mozL10n.get("tour_label")}
                                   onClick={this.openGettingStartedTour} />
            <SettingsDropdownEntry displayed={this.props.mozLoop.fxAEnabled}
                                   icon={this._isSignedIn() ? "signout" : "signin"}
                                   label={this._isSignedIn() ?
                                          mozL10n.get("settings_menu_item_signout") :
                                          mozL10n.get("settings_menu_item_signin")}
                                   onClick={this.handleClickAuthEntry} />
            <SettingsDropdownEntry icon="help"
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
  var AuthLink = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    handleSignUpLinkClick: function() {
      navigator.mozLoop.logInToFxA();
      this.closeWindow();
    },

    render: function() {
      if (!navigator.mozLoop.fxAEnabled || navigator.mozLoop.userProfile) {
        return null;
      }
      return (
        <p className="signin-link">
          <a href="#" onClick={this.handleSignUpLinkClick}>
            {mozL10n.get("panel_footer_signin_or_signup_link")}
          </a>
        </p>
      );
    }
  });

  /**
   * FxA user identity (guest/authenticated) component.
   */
  var UserIdentity = React.createClass({
    propTypes: {
      displayName: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        <p className="user-identity">
          {this.props.displayName}
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

    mixins: [loop.shared.mixins.WindowCloseMixin],

    getInitialState: function() {
      return { urlCopied: false };
    },

    shouldComponentUpdate: function(nextProps, nextState) {
      return (nextProps.room.ctime > this.props.room.ctime) ||
        (nextState.urlCopied !== this.state.urlCopied);
    },

    handleClickEntry: function(event) {
      event.preventDefault();
      this.props.dispatcher.dispatch(new sharedActions.OpenRoom({
        roomToken: this.props.room.roomToken
      }));
      this.closeWindow();
    },

    handleCopyButtonClick: function(event) {
      event.stopPropagation();
      event.preventDefault();
      this.props.dispatcher.dispatch(new sharedActions.CopyRoomUrl({
        roomUrl: this.props.room.roomUrl,
        from: "panel"
      }));
      this.setState({urlCopied: true});
    },

    handleDeleteButtonClick: function(event) {
      event.stopPropagation();
      event.preventDefault();
      this.props.mozLoop.confirm({
        message: mozL10n.get("rooms_list_deleteConfirmation_label"),
        okButton: null,
        cancelButton: null
      }, function(err, result) {
        if (err) {
          throw err;
        }

        if (!result) {
          return;
        }

        this.props.dispatcher.dispatch(new sharedActions.DeleteRoom({
          roomToken: this.props.room.roomToken
        }));
      }.bind(this));
    },

    handleMouseLeave: function(event) {
      this.setState({urlCopied: false});
    },

    _isActive: function() {
      return this.props.room.participants.length > 0;
    },

    render: function() {
      var roomClasses = React.addons.classSet({
        "room-entry": true,
        "room-active": this._isActive()
      });
      var copyButtonClasses = React.addons.classSet({
        "copy-link": true,
        "checked": this.state.urlCopied
      });

      return (
        <div className={roomClasses} onClick={this.handleClickEntry}
             onMouseLeave={this.handleMouseLeave}>
          <h2>
            <span className="room-notification" />
            {this.props.room.decryptedContext.roomName}
            <button className={copyButtonClasses}
              onClick={this.handleCopyButtonClick}
              title={mozL10n.get("rooms_list_copy_url_tooltip")} />
            <button className="delete-link"
              onClick={this.handleDeleteButtonClick}
              title={mozL10n.get("rooms_list_delete_tooltip")} />
          </h2>
          <RoomEntryContextItem mozLoop={this.props.mozLoop}
                                roomUrls={this.props.room.decryptedContext.urls} />
        </div>
      );
    }
  });

  /**
   * Room list.
   */
  var RoomList = React.createClass({
    mixins: [Backbone.Events, sharedMixins.WindowCloseMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      store: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      userDisplayName: React.PropTypes.string.isRequired  // for room creation
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

    _getListHeading: function() {
      var numRooms = this.state.rooms.length;
      if (numRooms === 0) {
        return mozL10n.get("rooms_list_no_current_conversations");
      }
      return mozL10n.get("rooms_list_current_conversations", {num: numRooms});
    },

    render: function() {
      if (this.state.error) {
        // XXX Better end user reporting of errors.
        console.error("RoomList error", this.state.error);
      }

      return (
        <div className="rooms">
          <h1>{this._getListHeading()}</h1>
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
          <NewRoomView dispatcher={this.props.dispatcher}
            mozLoop={this.props.mozLoop}
            pendingOperation={this.state.pendingCreation ||
              this.state.pendingInitialRetrieval}
            userDisplayName={this.props.userDisplayName} />
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
        nameTemplate: mozL10n.get("rooms_default_room_name_template"),
        roomOwner: this.props.userDisplayName
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
      mozLoop: React.PropTypes.object.isRequired,
      notifications: React.PropTypes.object.isRequired,
      roomStore:
        React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      selectedTab: React.PropTypes.string,
      // Used only for unit tests.
      showTabButtons: React.PropTypes.bool,
      // Mostly used for UI components showcase and unit tests
      userProfile: React.PropTypes.object
    },

    getInitialState: function() {
      return {
        hasEncryptionKey: this.props.mozLoop.hasEncryptionKey,
        userProfile: this.props.userProfile || this.props.mozLoop.userProfile,
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
      if (currUid == newUid) {
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

    startForm: function(name, contact) {
      this.refs[name].initForm(contact);
      this.selectTab(name);
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

    _getUserDisplayName: function() {
      return this.state.userProfile && this.state.userProfile.email ||
             mozL10n.get("display_name_guest");
    },

    render: function() {
      var NotificationListView = sharedViews.NotificationListView;

      if (!this.state.gettingStartedSeen) {
        return (
          <div>
            <NotificationListView
              clearOnDocumentHidden={true}
              notifications={this.props.notifications} />
            <GettingStartedView />
            <ToSView />
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
        <div>
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
                        userDisplayName={this._getUserDisplayName()} />
              <ToSView />
            </Tab>
            <Tab name="contacts">
              <ContactsList
                notifications={this.props.notifications}
                selectTab={this.selectTab}
                startForm={this.startForm} />
            </Tab>
            <Tab hidden={true} name="contacts_add">
              <ContactDetailsForm
                mode="add"
                ref="contacts_add"
                selectTab={this.selectTab} />
            </Tab>
            <Tab hidden={true} name="contacts_edit">
              <ContactDetailsForm
                mode="edit"
                ref="contacts_edit"
                selectTab={this.selectTab} />
            </Tab>
            <Tab hidden={true} name="contacts_import">
              <ContactDetailsForm
                mode="import"
                ref="contacts_import"
                selectTab={this.selectTab}/>
            </Tab>
          </TabView>
          <div className="footer">
            <div className="user-details">
              <UserIdentity displayName={this._getUserDisplayName()} />
              <AvailabilityDropdown />
            </div>
            <div className="signin-details">
              <AuthLink />
              <div className="footer-signin-separator" />
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
    init: init,
    AuthLink: AuthLink,
    AvailabilityDropdown: AvailabilityDropdown,
    GettingStartedView: GettingStartedView,
    NewRoomView: NewRoomView,
    PanelView: PanelView,
    RoomEntry: RoomEntry,
    RoomList: RoomList,
    SettingsDropdown: SettingsDropdown,
    SignInRequestView: SignInRequestView,
    ToSView: ToSView,
    UserIdentity: UserIdentity
  };
})(_, document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.panel.init);
