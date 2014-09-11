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
  var __ = mozL10n.get; // aliasing translation function as __ for concision

  /**
   * Panel router.
   * @type {loop.desktopRouter.DesktopRouter}
   */
  var router;

  var TabView = React.createClass({displayName: 'TabView',
    getInitialState: function() {
      return {
        selectedTab: "call"
      };
    },

    handleSelectTab: function(event) {
      var tabName = event.target.dataset.tabName;
      this.setState({selectedTab: tabName});

      if (this.props.onSelect) {
        this.props.onSelect(tabName);
      }
    },

    render: function() {
      var cx = React.addons.classSet;
      var tabButtons = [];
      var tabs = [];
      React.Children.forEach(this.props.children, function(tab, i) {
        var tabName = tab.props.name;
        var isSelected = (this.state.selectedTab == tabName);
        tabButtons.push(
          React.DOM.li({className: cx({selected: isSelected}), 
              key: i, 
              'data-tab-name': tabName, 
              onClick: this.handleSelectTab}
          )
        );
        tabs.push(
          React.DOM.div({key: i, className: cx({tab: true, selected: isSelected})}, 
            tab.props.children
          )
        );
      }, this);
      return (
        React.DOM.div({className: "tab-view-container"}, 
          React.DOM.ul({className: "tab-view"}, tabButtons), 
          tabs
        )
      );
    }
  });

  var Tab = React.createClass({displayName: 'Tab',
    render: function() {
      return null;
    }
  });

  /**
   * Availability drop down menu subview.
   */
  var AvailabilityDropdown = React.createClass({displayName: 'AvailabilityDropdown',
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
                              __("display_name_dnd_status") :
                              __("display_name_available_status");

      return (
        React.DOM.div({className: "dropdown"}, 
          React.DOM.p({className: "dnd-status", onClick: this.showDropdownMenu}, 
            React.DOM.span(null, availabilityText), 
            React.DOM.i({className: availabilityStatus})
          ), 
          React.DOM.ul({className: availabilityDropdown, 
              onMouseLeave: this.hideDropdownMenu}, 
            React.DOM.li({onClick: this.changeAvailability("available"), 
                className: "dropdown-menu-item dnd-make-available"}, 
              React.DOM.i({className: "status status-available"}), 
              React.DOM.span(null, __("display_name_available_status"))
            ), 
            React.DOM.li({onClick: this.changeAvailability("do-not-disturb"), 
                className: "dropdown-menu-item dnd-make-unavailable"}, 
              React.DOM.i({className: "status status-dnd"}), 
              React.DOM.span(null, __("display_name_dnd_status"))
            )
          )
        )
      );
    }
  });

  var ToSView = React.createClass({displayName: 'ToSView',
    getInitialState: function() {
      return {seenToS: navigator.mozLoop.getLoopCharPref('seenToS')};
    },

    render: function() {
      if (this.state.seenToS == "unseen") {
        var terms_of_use_url = navigator.mozLoop.getLoopCharPref('legal.ToS_url');
        var privacy_notice_url = navigator.mozLoop.getLoopCharPref('legal.privacy_url');
        var tosHTML = __("legal_text_and_links3", {
          "clientShortname": __("client_shortname_fallback"),
          "terms_of_use": React.renderComponentToStaticMarkup(
            React.DOM.a({href: terms_of_use_url, target: "_blank"}, 
              __("legal_text_tos")
            )
          ),
          "privacy_notice": React.renderComponentToStaticMarkup(
            React.DOM.a({href: privacy_notice_url, target: "_blank"}, 
              __("legal_text_privacy")
            )
          ),
        });
        return React.DOM.p({className: "terms-service", 
                  dangerouslySetInnerHTML: {__html: tosHTML}});
      } else {
        return React.DOM.div(null);
      }
    }
  });

  /**
   * Panel settings (gear) menu entry.
   */
  var SettingsDropdownEntry = React.createClass({displayName: 'SettingsDropdownEntry',
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
        React.DOM.li({onClick: this.props.onClick, className: "dropdown-menu-item"}, 
          this.props.icon ?
            React.DOM.i({className: "icon icon-" + this.props.icon}) :
            null, 
          React.DOM.span(null, this.props.label)
        )
      );
    }
  });

  /**
   * Panel settings (gear) menu.
   */
  var SettingsDropdown = React.createClass({displayName: 'SettingsDropdown',
    mixins: [sharedMixins.DropdownMenuMixin],

    handleClickSettingsEntry: function() {
      // XXX to be implemented
    },

    handleClickAccountEntry: function() {
      // XXX to be implemented
    },

    handleClickAuthEntry: function() {
      if (this._isSignedIn()) {
        // XXX to be implemented - bug 979845
        navigator.mozLoop.logOutFromFxA();
      } else {
        navigator.mozLoop.logInToFxA();
      }
    },

    _isSignedIn: function() {
      // XXX to be implemented - bug 979845
      return !!navigator.mozLoop.loggedInToFxA;
    },

    render: function() {
      var cx = React.addons.classSet;
      return (
        React.DOM.div({className: "settings-menu dropdown"}, 
          React.DOM.a({className: "btn btn-settings", onClick: this.showDropdownMenu, 
             title: __("settings_menu_button_tooltip")}), 
          React.DOM.ul({className: cx({"dropdown-menu": true, hide: !this.state.showMenu}), 
              onMouseLeave: this.hideDropdownMenu}, 
            SettingsDropdownEntry({label: __("settings_menu_item_settings"), 
                                   onClick: this.handleClickSettingsEntry, 
                                   icon: "settings"}), 
            SettingsDropdownEntry({label: __("settings_menu_item_account"), 
                                   onClick: this.handleClickAccountEntry, 
                                   icon: "account", 
                                   displayed: this._isSignedIn()}), 
            SettingsDropdownEntry({label: this._isSignedIn() ?
                                          __("settings_menu_item_signout") :
                                          __("settings_menu_item_signin"), 
                                   onClick: this.handleClickAuthEntry, 
                                   icon: this._isSignedIn() ? "signout" : "signin"})
          )
        )
      );
    }
  });

  /**
   * Panel layout.
   */
  var PanelLayout = React.createClass({displayName: 'PanelLayout',
    propTypes: {
      summary: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        React.DOM.div({className: "share generate-url"}, 
          React.DOM.div({className: "description"}, this.props.summary), 
          React.DOM.div({className: "action"}, 
            this.props.children
          )
        )
      );
    }
  });

  /**
   * Call url result view.
   */
  var CallUrlResult = React.createClass({displayName: 'CallUrlResult',
    mixins: [sharedMixins.DocumentVisibilityMixin],

    propTypes: {
      callUrl:        React.PropTypes.string,
      callUrlExpiry:  React.PropTypes.number,
      notifications:  React.PropTypes.object.isRequired,
      client:         React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return {
        pending: false,
        copied: false,
        callUrl: this.props.callUrl || "",
        callUrlExpiry: 0
      };
    },

    /**
     * Provided by DocumentVisibilityMixin. Schedules retrieval of a new call
     * URL everytime the panel is reopened.
     */
    onDocumentVisible: function() {
      this._fetchCallUrl();
    },

    /**
    * Returns a random 5 character string used to identify
    * the conversation.
    * XXX this will go away once the backend changes
    */
    conversationIdentifier: function() {
      return Math.random().toString(36).substring(5);
    },

    componentDidMount: function() {
      // If we've already got a callURL, don't bother requesting a new one.
      // As of this writing, only used for visual testing in the UI showcase.
      if (this.state.callUrl.length) {
        return;
      }

      this._fetchCallUrl();
    },

    /**
     * Fetches a call URL.
     */
    _fetchCallUrl: function() {
      this.setState({pending: true});
      this.props.client.requestCallUrl(this.conversationIdentifier(),
                                       this._onCallUrlReceived);
    },

    _onCallUrlReceived: function(err, callUrlData) {
      this.props.notifications.reset();

      if (err) {
        this.props.notifications.errorL10n("unable_retrieve_url");
        this.setState(this.getInitialState());
      } else {
        try {
          var callUrl = new window.URL(callUrlData.callUrl);
          // XXX the current server vers does not implement the callToken field
          // but it exists in the API. This workaround should be removed in the future
          var token = callUrlData.callToken ||
                      callUrl.pathname.split('/').pop();

          this.setState({pending: false, copied: false,
                         callUrl: callUrl.href,
                         callUrlExpiry: callUrlData.expiresAt});
        } catch(e) {
          console.log(e);
          this.props.notifications.errorL10n("unable_retrieve_url");
          this.setState(this.getInitialState());
        }
      }
    },

    _generateMailTo: function() {
      return encodeURI([
        "mailto:?subject=" + __("share_email_subject3") + "&",
        "body=" + __("share_email_body3", {callUrl: this.state.callUrl})
      ].join(""));
    },

    handleEmailButtonClick: function(event) {
      this.handleLinkExfiltration(event);
      // Note: side effect
      document.location = event.target.dataset.mailto;
    },

    handleCopyButtonClick: function(event) {
      this.handleLinkExfiltration(event);
      // XXX the mozLoop object should be passed as a prop, to ease testing and
      //     using a fake implementation in UI components showcase.
      navigator.mozLoop.copyString(this.state.callUrl);
      this.setState({copied: true});
    },

    handleLinkExfiltration: function(event) {
      // TODO Bug 1015988 -- Increase link exfiltration telemetry count
      if (this.state.callUrlExpiry) {
        navigator.mozLoop.noteCallUrlExpiry(this.state.callUrlExpiry);
      }
    },

    render: function() {
      // XXX setting elem value from a state (in the callUrl input)
      // makes it immutable ie read only but that is fine in our case.
      // readOnly attr will suppress a warning regarding this issue
      // from the react lib.
      var cx = React.addons.classSet;
      var inputCSSClass = cx({
        "pending": this.state.pending,
        // Used in functional testing, signals that
        // call url was received from loop server
         "callUrl": !this.state.pending
      });
      return (
        PanelLayout({summary: __("share_link_header_text")}, 
          React.DOM.div({className: "invite"}, 
            React.DOM.input({type: "url", value: this.state.callUrl, readOnly: "true", 
                   onCopy: this.handleLinkExfiltration, 
                   className: inputCSSClass}), 
            React.DOM.p({className: "btn-group url-actions"}, 
              React.DOM.button({className: "btn btn-email", disabled: !this.state.callUrl, 
                onClick: this.handleEmailButtonClick, 
                'data-mailto': this._generateMailTo()}, 
                __("share_button")
              ), 
              React.DOM.button({className: "btn btn-copy", disabled: !this.state.callUrl, 
                onClick: this.handleCopyButtonClick}, 
                this.state.copied ? __("copied_url_button") :
                                     __("copy_url_button")
              )
            )
          )
        )
      );
    }
  });

  /**
   * FxA sign in/up link component.
   */
  var AuthLink = React.createClass({displayName: 'AuthLink',
    handleSignUpLinkClick: function() {
      navigator.mozLoop.logInToFxA();
    },

    render: function() {
      if (navigator.mozLoop.loggedInToFxA) { // XXX to be implemented
        return null;
      }
      return (
        React.DOM.p({className: "signin-link"}, 
          React.DOM.a({href: "#", onClick: this.handleSignUpLinkClick}, 
            __("panel_footer_signin_or_signup_link")
          )
        )
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({displayName: 'PanelView',
    propTypes: {
      notifications: React.PropTypes.object.isRequired,
      client: React.PropTypes.object.isRequired,
      // Mostly used for UI components showcase and unit tests
      callUrl: React.PropTypes.string
    },

    render: function() {
      var NotificationListView = sharedViews.NotificationListView;

      return (
        React.DOM.div(null, 
          NotificationListView({notifications: this.props.notifications, 
                                clearOnDocumentHidden: true}), 
          TabView({onSelect: this.selectTab}, 
            Tab({name: "call"}, 
              CallUrlResult({client: this.props.client, 
                             notifications: this.props.notifications, 
                             callUrl: this.props.callUrl}), 
              ToSView(null)
            ), 
            Tab({name: "contacts"}, 
              React.DOM.span(null, "contacts")
            )
          ), 
          React.DOM.div({className: "footer"}, 
            AvailabilityDropdown(null), 
            AuthLink(null), 
            SettingsDropdown(null)
          )
        )
      );
    }
  });

  var PanelRouter = loop.desktopRouter.DesktopRouter.extend({
    /**
     * DOM document object.
     * @type {HTMLDocument}
     */
    document: undefined,

    routes: {
      "": "home"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.document) {
        throw new Error("missing required document");
      }
    },

    /**
     * Default entry point.
     */
    home: function() {
      this._notifications.reset();
      var client = new loop.Client({
        baseServerUrl: navigator.mozLoop.serverUrl
      });
      this.loadReactComponent(
          PanelView({client: client, notifications: this._notifications}));
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    router = new PanelRouter({
      document: document,
      notifications: new sharedModels.NotificationCollection()
    });
    Backbone.history.start();

    document.body.classList.add(loop.shared.utils.getTargetPlatform());
    document.body.setAttribute("dir", mozL10n.getDirection());

    // Notify the window that we've finished initalization and initial layout
    var evtObject = document.createEvent('Event');
    evtObject.initEvent('loopPanelInitialized', true, false);
    window.dispatchEvent(evtObject);
  }

  return {
    init: init,
    AvailabilityDropdown: AvailabilityDropdown,
    CallUrlResult: CallUrlResult,
    PanelView: PanelView,
    PanelRouter: PanelRouter,
    SettingsDropdown: SettingsDropdown,
    ToSView: ToSView
  };
})(_, document.mozL10n);
