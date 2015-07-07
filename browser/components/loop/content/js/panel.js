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

  var TabView = React.createClass({displayName: "TabView",
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
            React.createElement("li", {className: cx({selected: isSelected}), 
                "data-tab-name": tabName, 
                key: i, 
                onClick: this.handleSelectTab, 
                title: mozL10n.get(tabName + "_tab_button_tooltip")})
          );
        }
        tabs.push(
          React.createElement("div", {className: cx({tab: true, selected: isSelected}), key: i}, 
            tab.props.children
          )
        );
      }, this);
      return (
        React.createElement("div", {className: "tab-view-container"}, 
          React.createElement("ul", {className: "tab-view"}, tabButtons), 
          tabs
        )
      );
    }
  });

  var Tab = React.createClass({displayName: "Tab",
    render: function() {
      return null;
    }
  });

  /**
   * Availability drop down menu subview.
   */
  var AvailabilityDropdown = React.createClass({displayName: "AvailabilityDropdown",
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
        React.createElement("div", {className: "dropdown"}, 
          React.createElement("p", {className: "dnd-status", onClick: this.toggleDropdownMenu, ref: "menu-button"}, 
            React.createElement("span", null, availabilityText), 
            React.createElement("i", {className: availabilityStatus})
          ), 
          React.createElement("ul", {className: availabilityDropdown}, 
            React.createElement("li", {className: "dropdown-menu-item dnd-make-available", 
                onClick: this.changeAvailability("available")}, 
              React.createElement("i", {className: "status status-available"}), 
              React.createElement("span", null, mozL10n.get("display_name_available_status"))
            ), 
            React.createElement("li", {className: "dropdown-menu-item dnd-make-unavailable", 
                onClick: this.changeAvailability("do-not-disturb")}, 
              React.createElement("i", {className: "status status-dnd"}), 
              React.createElement("span", null, mozL10n.get("display_name_dnd_status"))
            )
          )
        )
      );
    }
  });

  var GettingStartedView = React.createClass({displayName: "GettingStartedView",
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
        React.createElement("div", {id: "fte-getstarted"}, 
          React.createElement("header", {id: "fte-title"}, 
            mozL10n.get("first_time_experience_title", {
              "clientShortname": mozL10n.get("clientShortname2")
            })
          ), 
          React.createElement(Button, {caption: mozL10n.get("first_time_experience_button_label"), 
                  htmlId: "fte-button", 
                  onClick: this.handleButtonClick})
        )
      );
    }
  });

  /**
   * Displays a view requesting the user to sign-in again.
   */
  var SignInRequestView = React.createClass({displayName: "SignInRequestView",
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
        React.createElement("div", {className: "sign-in-request"}, 
          React.createElement("h1", null, line1), 
          React.createElement("h2", null, line2), 
          React.createElement("div", null, 
            React.createElement("button", {className: "btn btn-info sign-in-request-button", 
                    onClick: this.handleSignInClick}, 
              mozL10n.get("sign_in_again_button")
            )
          ), 
          React.createElement("a", {onClick: this.handleGuestClick}, 
            useGuestString
          )
        )
      );
    }
  });

  var ToSView = React.createClass({displayName: "ToSView",
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
        React.createElement("p", {className: "powered-by", id: "powered-by"}, 
          mozL10n.get("powered_by_beforeLogo"), 
          React.createElement("img", {className: locale, id: "powered-by-logo"}), 
          mozL10n.get("powered_by_afterLogo")
        )
      );
    },

    render: function() {
      if (!this.state.gettingStartedSeen || this.state.seenToS == "unseen") {
        var terms_of_use_url = navigator.mozLoop.getLoopPref("legal.ToS_url");
        var privacy_notice_url = navigator.mozLoop.getLoopPref("legal.privacy_url");
        var tosHTML = mozL10n.get("legal_text_and_links3", {
          "clientShortname": mozL10n.get("clientShortname2"),
          "terms_of_use": React.renderToStaticMarkup(
            React.createElement("a", {href: terms_of_use_url, target: "_blank"}, 
              mozL10n.get("legal_text_tos")
            )
          ),
          "privacy_notice": React.renderToStaticMarkup(
            React.createElement("a", {href: privacy_notice_url, target: "_blank"}, 
              mozL10n.get("legal_text_privacy")
            )
          )
        });
        return (
          React.createElement("div", {id: "powered-by-wrapper"}, 
            this.renderPartnerLogo(), 
            React.createElement("p", {className: "terms-service", 
               dangerouslySetInnerHTML: {__html: tosHTML}, 
               onClick: this.handleLinkClick})
           )
        );
      } else {
        return React.createElement("div", null);
      }
    }
  });

  /**
   * Panel settings (gear) menu entry.
   */
  var SettingsDropdownEntry = React.createClass({displayName: "SettingsDropdownEntry",
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
        React.createElement("li", {className: "dropdown-menu-item", onClick: this.props.onClick}, 
          this.props.icon ?
            React.createElement("i", {className: "icon icon-" + this.props.icon}) :
            null, 
          React.createElement("span", null, this.props.label)
        )
      );
    }
  });

  /**
   * Panel settings (gear) menu.
   */
  var SettingsDropdown = React.createClass({displayName: "SettingsDropdown",
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
        React.createElement("div", {className: "settings-menu dropdown"}, 
          React.createElement("a", {className: "button-settings", 
             onClick: this.toggleDropdownMenu, 
             ref: "menu-button", 
             title: mozL10n.get("settings_menu_button_tooltip")}), 
          React.createElement("ul", {className: cx({"dropdown-menu": true, hide: !this.state.showMenu})}, 
            React.createElement(SettingsDropdownEntry, {displayed: false, 
                                   icon: "settings", 
                                   label: mozL10n.get("settings_menu_item_settings"), 
                                   onClick: this.handleClickSettingsEntry}), 
            React.createElement(SettingsDropdownEntry, {displayed: this._isSignedIn() && this.props.mozLoop.fxAEnabled, 
                                   icon: "account", 
                                   label: mozL10n.get("settings_menu_item_account"), 
                                   onClick: this.handleClickAccountEntry}), 
            React.createElement(SettingsDropdownEntry, {icon: "tour", 
                                   label: mozL10n.get("tour_label"), 
                                   onClick: this.openGettingStartedTour}), 
            React.createElement(SettingsDropdownEntry, {displayed: this.props.mozLoop.fxAEnabled, 
                                   icon: this._isSignedIn() ? "signout" : "signin", 
                                   label: this._isSignedIn() ?
                                          mozL10n.get("settings_menu_item_signout") :
                                          mozL10n.get("settings_menu_item_signin"), 
                                   onClick: this.handleClickAuthEntry}), 
            React.createElement(SettingsDropdownEntry, {icon: "help", 
                                   label: mozL10n.get("help_label"), 
                                   onClick: this.handleHelpEntry})
          )
        )
      );
    }
  });

  /**
   * FxA sign in/up link component.
   */
  var AuthLink = React.createClass({displayName: "AuthLink",
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
        React.createElement("p", {className: "signin-link"}, 
          React.createElement("a", {href: "#", onClick: this.handleSignUpLinkClick}, 
            mozL10n.get("panel_footer_signin_or_signup_link")
          )
        )
      );
    }
  });

  /**
   * FxA user identity (guest/authenticated) component.
   */
  var UserIdentity = React.createClass({displayName: "UserIdentity",
    propTypes: {
      displayName: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        React.createElement("p", {className: "user-identity"}, 
          this.props.displayName
        )
      );
    }
  });

  var RoomEntryContextItem = React.createClass({displayName: "RoomEntryContextItem",
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
        React.createElement("div", {className: "room-entry-context-item"}, 
          React.createElement("a", {href: roomUrl.location, onClick: this.handleClick, title: roomUrl.description}, 
            React.createElement("img", {src: roomUrl.thumbnail || "loop/shared/img/icons-16x16.svg#globe"})
          )
        )
      );
    }
  });

  /**
   * Room list entry.
   */
  var RoomEntry = React.createClass({displayName: "RoomEntry",
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
        React.createElement("div", {className: roomClasses, onClick: this.handleClickEntry, 
             onMouseLeave: this.handleMouseLeave}, 
          React.createElement("h2", null, 
            React.createElement("span", {className: "room-notification"}), 
            this.props.room.decryptedContext.roomName, 
            React.createElement("button", {className: copyButtonClasses, 
              onClick: this.handleCopyButtonClick, 
              title: mozL10n.get("rooms_list_copy_url_tooltip")}), 
            React.createElement("button", {className: "delete-link", 
              onClick: this.handleDeleteButtonClick, 
              title: mozL10n.get("rooms_list_delete_tooltip")})
          ), 
          React.createElement(RoomEntryContextItem, {mozLoop: this.props.mozLoop, 
                                roomUrls: this.props.room.decryptedContext.urls})
        )
      );
    }
  });

  /**
   * Room list.
   */
  var RoomList = React.createClass({displayName: "RoomList",
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
        React.createElement("div", {className: "rooms"}, 
          React.createElement("h1", null, this._getListHeading()), 
          React.createElement("div", {className: "room-list"}, 
            this.state.rooms.map(function(room, i) {
              return (
                React.createElement(RoomEntry, {
                  dispatcher: this.props.dispatcher, 
                  key: room.roomToken, 
                  mozLoop: this.props.mozLoop, 
                  room: room})
              );
            }, this)
          ), 
          React.createElement(NewRoomView, {dispatcher: this.props.dispatcher, 
            mozLoop: this.props.mozLoop, 
            pendingOperation: this.state.pendingCreation ||
              this.state.pendingInitialRetrieval, 
            userDisplayName: this.props.userDisplayName})
        )
      );
    }
  });

  /**
   * Used for creating a new room with or without context.
   */
  var NewRoomView = React.createClass({displayName: "NewRoomView",
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
        React.createElement("div", {className: "new-room-view"}, 
          React.createElement("div", {className: contextClasses}, 
            React.createElement(Checkbox, {checked: this.state.checked, 
                      label: mozL10n.get("context_inroom_label"), 
                      onChange: this.onCheckboxChange}), 
            React.createElement(sharedViews.ContextUrlView, {
              allowClick: false, 
              description: this.state.description, 
              showContextTitle: false, 
              thumbnail: this.state.previewImage, 
              url: this.state.url, 
              useDesktopPaths: true})
          ), 
          React.createElement("button", {className: "btn btn-info new-room-button", 
                  disabled: this.props.pendingOperation, 
                  onClick: this.handleCreateButtonClick}, 
            mozL10n.get("rooms_new_room_button_label")
          )
        )
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({displayName: "PanelView",
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
          React.createElement("div", null, 
            React.createElement(NotificationListView, {
              clearOnDocumentHidden: true, 
              notifications: this.props.notifications}), 
            React.createElement(GettingStartedView, null), 
            React.createElement(ToSView, null)
          )
        );
      }

      if (!this.state.hasEncryptionKey) {
        return React.createElement(SignInRequestView, {mozLoop: this.props.mozLoop});
      }

      // Determine which buttons to NOT show.
      var hideButtons = [];
      if (!this.state.userProfile && !this.props.showTabButtons) {
        hideButtons.push("contacts");
      }

      return (
        React.createElement("div", null, 
          React.createElement(NotificationListView, {
            clearOnDocumentHidden: true, 
            notifications: this.props.notifications}), 
          React.createElement(TabView, {
            buttonsHidden: hideButtons, 
            mozLoop: this.props.mozLoop, 
            ref: "tabView", 
            selectedTab: this.props.selectedTab}, 
            React.createElement(Tab, {name: "rooms"}, 
              React.createElement(RoomList, {dispatcher: this.props.dispatcher, 
                        mozLoop: this.props.mozLoop, 
                        store: this.props.roomStore, 
                        userDisplayName: this._getUserDisplayName()}), 
              React.createElement(ToSView, null)
            ), 
            React.createElement(Tab, {name: "contacts"}, 
              React.createElement(ContactsList, {
                notifications: this.props.notifications, 
                selectTab: this.selectTab, 
                startForm: this.startForm})
            ), 
            React.createElement(Tab, {hidden: true, name: "contacts_add"}, 
              React.createElement(ContactDetailsForm, {
                mode: "add", 
                ref: "contacts_add", 
                selectTab: this.selectTab})
            ), 
            React.createElement(Tab, {hidden: true, name: "contacts_edit"}, 
              React.createElement(ContactDetailsForm, {
                mode: "edit", 
                ref: "contacts_edit", 
                selectTab: this.selectTab})
            ), 
            React.createElement(Tab, {hidden: true, name: "contacts_import"}, 
              React.createElement(ContactDetailsForm, {
                mode: "import", 
                ref: "contacts_import", 
                selectTab: this.selectTab})
            )
          ), 
          React.createElement("div", {className: "footer"}, 
            React.createElement("div", {className: "user-details"}, 
              React.createElement(UserIdentity, {displayName: this._getUserDisplayName()}), 
              React.createElement(AvailabilityDropdown, null)
            ), 
            React.createElement("div", {className: "signin-details"}, 
              React.createElement(AuthLink, null), 
              React.createElement("div", {className: "footer-signin-separator"}), 
              React.createElement(SettingsDropdown, {mozLoop: this.props.mozLoop})
            )
          )
        )
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

    React.render(React.createElement(PanelView, {
      dispatcher: dispatcher, 
      mozLoop: navigator.mozLoop, 
      notifications: notifications, 
      roomStore: roomStore}), document.querySelector("#main"));

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
