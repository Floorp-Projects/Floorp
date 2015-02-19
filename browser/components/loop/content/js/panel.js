/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop:true, React */

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
  var ContactsList = loop.contacts.ContactsList;
  var ContactDetailsForm = loop.contacts.ContactDetailsForm;

  var TabView = React.createClass({displayName: "TabView",
    propTypes: {
      buttonsHidden: React.PropTypes.array,
      // The selectedTab prop is used by the UI showcase.
      selectedTab: React.PropTypes.string,
      mozLoop: React.PropTypes.object
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
                key: i, 
                "data-tab-name": tabName, 
                title: mozL10n.get(tabName + "_tab_button_tooltip"), 
                onClick: this.handleSelectTab})
          );
        }
        tabs.push(
          React.createElement("div", {key: i, className: cx({tab: true, selected: isSelected})}, 
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
    mixins: [sharedMixins.DropdownMenuMixin],

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
          case 'available':
            this.setState({doNotDisturb: false});
            navigator.mozLoop.doNotDisturb = false;
            break;
          case 'do-not-disturb':
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
        'status': true,
        'status-dnd': this.state.doNotDisturb,
        'status-available': !this.state.doNotDisturb
      });
      var availabilityDropdown = cx({
        'dropdown-menu': true,
        'hide': !this.state.showMenu
      });
      var availabilityText = this.state.doNotDisturb ?
                              mozL10n.get("display_name_dnd_status") :
                              mozL10n.get("display_name_available_status");

      return (
        React.createElement("div", {className: "dropdown"}, 
          React.createElement("p", {className: "dnd-status", onClick: this.showDropdownMenu}, 
            React.createElement("span", null, availabilityText), 
            React.createElement("i", {className: availabilityStatus})
          ), 
          React.createElement("ul", {className: availabilityDropdown, 
              onMouseLeave: this.hideDropdownMenu}, 
            React.createElement("li", {onClick: this.changeAvailability("available"), 
                className: "dropdown-menu-item dnd-make-available"}, 
              React.createElement("i", {className: "status status-available"}), 
              React.createElement("span", null, mozL10n.get("display_name_available_status"))
            ), 
            React.createElement("li", {onClick: this.changeAvailability("do-not-disturb"), 
                className: "dropdown-menu-item dnd-make-unavailable"}, 
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
          React.createElement(Button, {htmlId: "fte-button", 
                  onClick: this.handleButtonClick, 
                  caption: mozL10n.get("first_time_experience_button_label")})
        )
      );
    }
  });

  var ToSView = React.createClass({displayName: "ToSView",
    getInitialState: function() {
      var getPref = navigator.mozLoop.getLoopPref.bind(navigator.mozLoop);

      return {
        seenToS: getPref("seenToS"),
        gettingStartedSeen: getPref("gettingStarted.seen"),
        showPartnerLogo: getPref("showPartnerLogo")
      };
    },

    renderPartnerLogo: function() {
      if (!this.state.showPartnerLogo) {
        return null;
      }

      var locale = mozL10n.getLanguage();
      navigator.mozLoop.setLoopPref('showPartnerLogo', false);
      return (
        React.createElement("p", {id: "powered-by", className: "powered-by"}, 
          mozL10n.get("powered_by_beforeLogo"), 
          React.createElement("img", {id: "powered-by-logo", className: locale}), 
          mozL10n.get("powered_by_afterLogo")
        )
      );
    },

    render: function() {
      if (!this.state.gettingStartedSeen || this.state.seenToS == "unseen") {
        var terms_of_use_url = navigator.mozLoop.getLoopPref('legal.ToS_url');
        var privacy_notice_url = navigator.mozLoop.getLoopPref('legal.privacy_url');
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
          ),
        });
        return React.createElement("div", {id: "powered-by-wrapper"}, 
          this.renderPartnerLogo(), 
          React.createElement("p", {className: "terms-service", 
             dangerouslySetInnerHTML: {__html: tosHTML}})
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
      onClick: React.PropTypes.func.isRequired,
      label: React.PropTypes.string.isRequired,
      icon: React.PropTypes.string,
      displayed: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {displayed: true};
    },

    render: function() {
      if (!this.props.displayed) {
        return null;
      }
      return (
        React.createElement("li", {onClick: this.props.onClick, className: "dropdown-menu-item"}, 
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
    mixins: [sharedMixins.DropdownMenuMixin, sharedMixins.WindowCloseMixin],

    handleClickSettingsEntry: function() {
      // XXX to be implemented at the same time as unhiding the entry
    },

    handleClickAccountEntry: function() {
      navigator.mozLoop.openFxASettings();
    },

    handleClickAuthEntry: function() {
      if (this._isSignedIn()) {
        navigator.mozLoop.logOutFromFxA();
      } else {
        navigator.mozLoop.logInToFxA();
      }
    },

    handleHelpEntry: function(event) {
      event.preventDefault();
      var helloSupportUrl = navigator.mozLoop.getLoopPref('support_url');
      window.open(helloSupportUrl);
      window.close();
    },

    _isSignedIn: function() {
      return !!navigator.mozLoop.userProfile;
    },

    openGettingStartedTour: function() {
      navigator.mozLoop.openGettingStartedTour("settings-menu");
      this.closeWindow();
    },

    render: function() {
      var cx = React.addons.classSet;

      return (
        React.createElement("div", {className: "settings-menu dropdown"}, 
          React.createElement("a", {className: "button-settings", onClick: this.showDropdownMenu, 
             title: mozL10n.get("settings_menu_button_tooltip")}), 
          React.createElement("ul", {className: cx({"dropdown-menu": true, hide: !this.state.showMenu}), 
              onMouseLeave: this.hideDropdownMenu}, 
            React.createElement(SettingsDropdownEntry, {label: mozL10n.get("settings_menu_item_settings"), 
                                   onClick: this.handleClickSettingsEntry, 
                                   displayed: false, 
                                   icon: "settings"}), 
            React.createElement(SettingsDropdownEntry, {label: mozL10n.get("settings_menu_item_account"), 
                                   onClick: this.handleClickAccountEntry, 
                                   icon: "account", 
                                   displayed: this._isSignedIn() && navigator.mozLoop.fxAEnabled}), 
            React.createElement(SettingsDropdownEntry, {icon: "tour", 
                                   label: mozL10n.get("tour_label"), 
                                   onClick: this.openGettingStartedTour}), 
            React.createElement(SettingsDropdownEntry, {label: this._isSignedIn() ?
                                          mozL10n.get("settings_menu_item_signout") :
                                          mozL10n.get("settings_menu_item_signin"), 
                                   onClick: this.handleClickAuthEntry, 
                                   displayed: navigator.mozLoop.fxAEnabled, 
                                   icon: this._isSignedIn() ? "signout" : "signin"}), 
            React.createElement(SettingsDropdownEntry, {label: mozL10n.get("help_label"), 
                                   onClick: this.handleHelpEntry, 
                                   icon: "help"})
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
    render: function() {
      return (
        React.createElement("p", {className: "user-identity"}, 
          this.props.displayName
        )
      );
    }
  });

  var EditInPlace = React.createClass({displayName: "EditInPlace",
    mixins: [React.addons.LinkedStateMixin],

    propTypes: {
      onChange: React.PropTypes.func.isRequired,
      text: React.PropTypes.string,
    },

    getDefaultProps: function() {
      return {text: ""};
    },

    getInitialState: function() {
      return {edit: false, text: this.props.text};
    },

    componentWillReceiveProps: function(nextProps) {
      if (nextProps.text !== this.props.text) {
        this.setState({text: nextProps.text});
      }
    },

    handleTextClick: function(event) {
      event.stopPropagation();
      event.preventDefault();
      this.setState({edit: true}, function() {
        this.getDOMNode().querySelector("input").select();
      }.bind(this));
    },

    handleInputClick: function(event) {
      event.stopPropagation();
    },

    handleFormSubmit: function(event) {
      event.preventDefault();
      // While we already validate for a non-empty string in the store, we need
      // to check it at the component level to avoid desynchronized rendering
      // issues.
      if (this.state.text.trim()) {
        this.props.onChange(this.state.text);
      } else {
        this.setState({text: this.props.text});
      }
      this.setState({edit: false});
    },

    cancelEdit: function(event) {
      event.stopPropagation();
      event.preventDefault();
      this.setState({edit: false, text: this.props.text});
    },

    render: function() {
      if (!this.state.edit) {
        return (
          React.createElement("span", {className: "edit-in-place", onClick: this.handleTextClick, 
                title: mozL10n.get("rooms_name_this_room_tooltip2")}, 
            this.state.text
          )
        );
      }
      return (
        React.createElement("form", {onSubmit: this.handleFormSubmit}, 
          React.createElement("input", {type: "text", valueLink: this.linkState("text"), 
                 onClick: this.handleInputClick, 
                 onBlur: this.cancelEdit})
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
      room:       React.PropTypes.instanceOf(loop.store.Room).isRequired
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
        roomUrl: this.props.room.roomUrl
      }));
      this.setState({urlCopied: true});
    },

    handleDeleteButtonClick: function(event) {
      event.stopPropagation();
      event.preventDefault();
      navigator.mozLoop.confirm({
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

    renameRoom: function(newRoomName) {
      this.props.dispatcher.dispatch(new sharedActions.RenameRoom({
        roomToken: this.props.room.roomToken,
        newRoomName: newRoomName
      }));
    },

    handleMouseLeave: function(event) {
      this.setState({urlCopied: false});
    },

    _isActive: function() {
      return this.props.room.participants.length > 0;
    },

    render: function() {
      var room = this.props.room;
      var roomClasses = React.addons.classSet({
        "room-entry": true,
        "room-active": this._isActive()
      });
      var copyButtonClasses = React.addons.classSet({
        "copy-link": true,
        "checked": this.state.urlCopied
      });

      return (
        React.createElement("div", {className: roomClasses, onMouseLeave: this.handleMouseLeave, 
             onClick: this.handleClickEntry}, 
          React.createElement("h2", null, 
            React.createElement("span", {className: "room-notification"}), 
            React.createElement(EditInPlace, {text: room.roomName, onChange: this.renameRoom}), 
            React.createElement("button", {className: copyButtonClasses, 
              title: mozL10n.get("rooms_list_copy_url_tooltip"), 
              onClick: this.handleCopyButtonClick}), 
            React.createElement("button", {className: "delete-link", 
              title: mozL10n.get("rooms_list_delete_tooltip"), 
              onClick: this.handleDeleteButtonClick})
          ), 
          React.createElement("p", null, React.createElement("a", {className: "room-url-link", href: "#"}, room.roomUrl))
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
      store: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
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

    _hasPendingOperation: function() {
      return this.state.pendingCreation || this.state.pendingInitialRetrieval;
    },

    handleCreateButtonClick: function() {
      this.props.dispatcher.dispatch(new sharedActions.CreateRoom({
        nameTemplate: mozL10n.get("rooms_default_room_name_template"),
        roomOwner: this.props.userDisplayName
      }));
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
              return React.createElement(RoomEntry, {
                key: room.roomToken, 
                dispatcher: this.props.dispatcher, 
                room: room}
              );
            }, this)
          ), 
          React.createElement("p", null, 
            React.createElement("button", {className: "btn btn-info new-room-button", 
                    onClick: this.handleCreateButtonClick, 
                    disabled: this._hasPendingOperation()}, 
              mozL10n.get("rooms_new_room_button_label")
            )
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
      notifications: React.PropTypes.object.isRequired,
      // Mostly used for UI components showcase and unit tests
      userProfile: React.PropTypes.object,
      // Used only for unit tests.
      showTabButtons: React.PropTypes.bool,
      selectedTab: React.PropTypes.string,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object,
      roomStore:
        React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    getInitialState: function() {
      return {
        userProfile: this.props.userProfile || this.props.mozLoop.userProfile,
        gettingStartedSeen: this.props.mozLoop.getLoopPref("gettingStarted.seen"),
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
        error: this.props.mozLoop.errors[firstErrorKey],
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
          detailsButtonCallback: serviceError.error.friendlyDetailsButtonCallback,
        });
      } else {
        this.props.notifications.remove(this.props.notifications.get("service-error"));
      }
    },

    _onStatusChanged: function() {
      var profile = this.props.mozLoop.userProfile;
      var currUid = this.state.userProfile ? this.state.userProfile.uid : null;
      var newUid = profile ? profile.uid : null;
      if (currUid != newUid) {
        // On profile change (login, logout), switch back to the default tab.
        this.selectTab("rooms");
        this.setState({userProfile: profile});
      }
      this.updateServiceErrors();
    },

    _gettingStartedSeen: function() {
      this.setState({
        gettingStartedSeen: this.props.mozLoop.getLoopPref("gettingStarted.seen"),
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
      this.refs.tabView.setState({ selectedTab: name });
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
            React.createElement(NotificationListView, {notifications: this.props.notifications, 
                                  clearOnDocumentHidden: true}), 
            React.createElement(GettingStartedView, null), 
            React.createElement(ToSView, null)
          )
        );
      }

      // Determine which buttons to NOT show.
      var hideButtons = [];
      if (!this.state.userProfile && !this.props.showTabButtons) {
        hideButtons.push("contacts");
      }

      return (
        React.createElement("div", null, 
          React.createElement(NotificationListView, {notifications: this.props.notifications, 
                                clearOnDocumentHidden: true}), 
          React.createElement(TabView, {ref: "tabView", selectedTab: this.props.selectedTab, 
            buttonsHidden: hideButtons, mozLoop: this.props.mozLoop}, 
            React.createElement(Tab, {name: "rooms"}, 
              React.createElement(RoomList, {dispatcher: this.props.dispatcher, 
                        store: this.props.roomStore, 
                        userDisplayName: this._getUserDisplayName()}), 
              React.createElement(ToSView, null)
            ), 
            React.createElement(Tab, {name: "contacts"}, 
              React.createElement(ContactsList, {selectTab: this.selectTab, 
                            startForm: this.startForm, 
                            notifications: this.props.notifications})
            ), 
            React.createElement(Tab, {name: "contacts_add", hidden: true}, 
              React.createElement(ContactDetailsForm, {ref: "contacts_add", mode: "add", 
                                  selectTab: this.selectTab})
            ), 
            React.createElement(Tab, {name: "contacts_edit", hidden: true}, 
              React.createElement(ContactDetailsForm, {ref: "contacts_edit", mode: "edit", 
                                  selectTab: this.selectTab})
            ), 
            React.createElement(Tab, {name: "contacts_import", hidden: true}, 
              React.createElement(ContactDetailsForm, {ref: "contacts_import", mode: "import", 
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
              React.createElement(SettingsDropdown, null)
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
      notifications: notifications, 
      roomStore: roomStore, 
      mozLoop: navigator.mozLoop, 
      dispatcher: dispatcher}
    ), document.querySelector("#main"));

    document.body.setAttribute("dir", mozL10n.getDirection());

    // Notify the window that we've finished initalization and initial layout
    var evtObject = document.createEvent('Event');
    evtObject.initEvent('loopPanelInitialized', true, false);
    window.dispatchEvent(evtObject);
  }

  return {
    init: init,
    AuthLink: AuthLink,
    AvailabilityDropdown: AvailabilityDropdown,
    GettingStartedView: GettingStartedView,
    PanelView: PanelView,
    RoomEntry: RoomEntry,
    RoomList: RoomList,
    SettingsDropdown: SettingsDropdown,
    ToSView: ToSView,
    UserIdentity: UserIdentity,
  };
})(_, document.mozL10n);

document.addEventListener('DOMContentLoaded', loop.panel.init);
