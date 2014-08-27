/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */
/* jshint newcap:false */

var loop = loop || {};
loop.webapp = (function($, _, OT, webL10n) {
  "use strict";

  loop.config = loop.config || {};
  loop.config.serverUrl = loop.config.serverUrl || "http://localhost:5000";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      baseServerUrl = loop.config.serverUrl,
      __ = webL10n.get;

  /**
   * App router.
   * @type {loop.webapp.WebappRouter}
   */
  var router;

  /**
   * Homepage view.
   */
  var HomeView = sharedViews.BaseView.extend({
    template: _.template('<p data-l10n-id="welcome"></p>')
  });

  /**
   * Firefox promotion interstitial. Will display only to non-Firefox users.
   */
  var PromoteFirefoxView = React.createClass({displayName: 'PromoteFirefoxView',
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      if (this.props.helper.isFirefox(navigator.userAgent)) {
        return React.DOM.div(null);
      }
      return (
        React.DOM.div({className: "promote-firefox"}, 
          React.DOM.h3(null, __("promote_firefox_hello_heading")), 
          React.DOM.p(null, 
            React.DOM.a({className: "btn btn-large btn-accept", 
               href: "https://www.mozilla.org/firefox/"}, 
              __("get_firefox_button")
            )
          )
        )
      );
    }
  });

  /**
   * Expired call URL view.
   */
  var CallUrlExpiredView = React.createClass({displayName: 'CallUrlExpiredView',
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      /* jshint ignore:start */
      return (
        React.DOM.div({className: "expired-url-info"}, 
          React.DOM.div({className: "info-panel"}, 
            React.DOM.div({className: "firefox-logo"}), 
            React.DOM.h1(null, __("call_url_unavailable_notification_heading")), 
            React.DOM.h4(null, __("call_url_unavailable_notification_message"))
          ), 
          PromoteFirefoxView({helper: this.props.helper})
        )
      );
      /* jshint ignore:end */
    }
  });

  var ConversationHeader = React.createClass({displayName: 'ConversationHeader',
    render: function() {
      var cx = React.addons.classSet;
      var conversationUrl = location.href;

      var urlCreationDateClasses = cx({
        "light-color-font": true,
        "call-url-date": true, /* Used as a handler in the tests */
        /*hidden until date is available*/
        "hide": !this.props.urlCreationDateString.length
      });

      var callUrlCreationDateString = __("call_url_creation_date_label", {
        "call_url_creation_date": this.props.urlCreationDateString
      });

      return (
        /* jshint ignore:start */
        React.DOM.header({className: "standalone-header container-box"}, 
          React.DOM.h1({className: "standalone-header-title"}, 
            React.DOM.strong(null, __("brandShortname")), " ", __("clientShortname")
          ), 
          React.DOM.div({className: "loop-logo", title: "Firefox WebRTC! logo"}), 
          React.DOM.h3({className: "call-url"}, 
            conversationUrl
          ), 
          React.DOM.h4({className: urlCreationDateClasses}, 
            callUrlCreationDateString
          )
        )
        /* jshint ignore:end */
      );
    }
  });

  var ConversationFooter = React.createClass({displayName: 'ConversationFooter',
    render: function() {
      return (
        React.DOM.div({className: "standalone-footer container-box"}, 
          React.DOM.div({title: "Mozilla Logo", className: "footer-logo"})
        )
      );
    }
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var StartConversationView = React.createClass({displayName: 'StartConversationView',
    /**
     * Constructor.
     *
     * Required options:
     * - {loop.shared.model.ConversationModel}    model    Conversation model.
     * - {loop.shared.views.NotificationListView} notifier Notifier component.
     *
     */

    getInitialState: function() {
      return {
        urlCreationDateString: '',
        disableCallButton: false,
        showCallOptionsMenu: false
      };
    },

    propTypes: {
      model: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                                       .isRequired,
      // XXX Check more tightly here when we start injecting window.loop.*
      notifier: React.PropTypes.object.isRequired,
      client: React.PropTypes.object.isRequired
    },

    componentDidMount: function() {
      // Listen for events & hide dropdown menu if user clicks away
      window.addEventListener("click", this.clickHandler);
      this.props.model.listenTo(this.props.model, "session:error",
                                this._onSessionError);
      this.props.client.requestCallUrlInfo(this.props.model.get("loopToken"),
                                           this._setConversationTimestamp);
      // XXX DOM element does not exist before React view gets instantiated
      // We should turn the notifier into a react component
      this.props.notifier.$el = $("#messages");
    },

    _onSessionError: function(error) {
      console.error(error);
      this.props.notifier.errorL10n("unable_retrieve_call_info");
    },

    /**
     * Initiates the call.
     * Takes in a call type parameter "audio" or "audio-video" and returns
     * a function that initiates the call. React click handler requires a function
     * to be called when that event happenes.
     *
     * @param {string} User call type choice "audio" or "audio-video"
     */
    _initiateOutgoingCall: function(callType) {
      return function() {
        this.props.model.set("selectedCallType", callType);
        this.setState({disableCallButton: true});
        this.props.model.setupOutgoingCall();
      }.bind(this);
    },

    _setConversationTimestamp: function(err, callUrlInfo) {
      if (err) {
        this.props.notifier.errorL10n("unable_retrieve_call_info");
      } else {
        var date = (new Date(callUrlInfo.urlCreationDate * 1000));
        var options = {year: "numeric", month: "long", day: "numeric"};
        var timestamp = date.toLocaleDateString(navigator.language, options);
        this.setState({urlCreationDateString: timestamp});
      }
    },

    componentWillUnmount: function() {
      window.removeEventListener("click", this.clickHandler);
      localStorage.setItem("has-seen-tos", "true");
    },

    clickHandler: function(e) {
      if (!e.target.classList.contains('btn-chevron') &&
          this.state.showCallOptionsMenu) {
            this._toggleCallOptionsMenu();
      }
    },

    _toggleCallOptionsMenu: function() {
      var state = this.state.showCallOptionsMenu;
      this.setState({showCallOptionsMenu: !state});
    },

    render: function() {
      var tos_link_name = __("terms_of_use_link_text");
      var privacy_notice_name = __("privacy_notice_link_text");

      var tosHTML = __("legal_text_and_links", {
        "terms_of_use_url": "<a target=_blank href='" +
          "https://accounts.firefox.com/legal/terms'>" + tos_link_name + "</a>",
        "privacy_notice_url": "<a target=_blank href='" +
          "https://www.mozilla.org/privacy/'>" + privacy_notice_name + "</a>"
      });

      var btnClassStartCall = "btn btn-large btn-accept " +
                              loop.shared.utils.getTargetPlatform();
      var dropdownMenuClasses = React.addons.classSet({
        "native-dropdown-large-parent": true,
        "standalone-dropdown-menu": true,
        "visually-hidden": !this.state.showCallOptionsMenu
      });
      var tosClasses = React.addons.classSet({
        "terms-service": true,
        hide: (localStorage.getItem("has-seen-tos") === "true")
      });

      return (
        /* jshint ignore:start */
        React.DOM.div({className: "container"}, 
          React.DOM.div({className: "container-box"}, 

            ConversationHeader({
              urlCreationDateString: this.state.urlCreationDateString}), 

            React.DOM.p({className: "standalone-call-btn-label"}, 
              __("initiate_call_button_label")
            ), 

            React.DOM.div({id: "messages"}), 

            React.DOM.div({className: "btn-group"}, 
              React.DOM.div({className: "flex-padding-1"}), 
              React.DOM.div({className: "standalone-btn-chevron-menu-group"}, 
                React.DOM.div({className: "btn-group-chevron"}, 
                  React.DOM.div({className: "btn-group"}, 

                    React.DOM.button({className: btnClassStartCall, 
                            onClick: this._initiateOutgoingCall("audio-video"), 
                            disabled: this.state.disableCallButton, 
                            title: __("initiate_audio_video_call_tooltip")}, 
                      React.DOM.span({className: "standalone-call-btn-text"}, 
                        __("initiate_audio_video_call_button")
                      ), 
                      React.DOM.span({className: "standalone-call-btn-video-icon"})
                    ), 

                    React.DOM.div({className: "btn-chevron", 
                      onClick: this._toggleCallOptionsMenu}
                    )

                  ), 

                  React.DOM.ul({className: dropdownMenuClasses}, 
                    React.DOM.li(null, 
                      /*
                       Button required for disabled state.
                       */
                      React.DOM.button({className: "start-audio-only-call", 
                              onClick: this._initiateOutgoingCall("audio"), 
                              disabled: this.state.disableCallButton}, 
                        __("initiate_audio_call_button")
                      )
                    )
                  )

                )
              ), 
              React.DOM.div({className: "flex-padding-1"})
            ), 

            React.DOM.p({className: tosClasses, 
               dangerouslySetInnerHTML: {__html: tosHTML}})
          ), 

          ConversationFooter(null)
        )
        /* jshint ignore:end */
      );
    }
  });

  /**
   * Webapp Router.
   */
  var WebappRouter = loop.shared.router.BaseConversationRouter.extend({
    routes: {
      "":                    "home",
      "unsupportedDevice":   "unsupportedDevice",
      "unsupportedBrowser":  "unsupportedBrowser",
      "call/expired":        "expired",
      "call/ongoing/:token": "loadConversation",
      "call/:token":         "initiate"
    },

    initialize: function(options) {
      this.helper = options.helper;
      if (!this.helper) {
        throw new Error("WebappRouter requires a helper object");
      }

      // Load default view
      this.loadView(new HomeView());

      this.listenTo(this._conversation, "timeout", this._onTimeout);
    },

    _onSessionExpired: function() {
      this.navigate("/call/expired", {trigger: true});
    },

    /**
     * Starts the set up of a call, obtaining the required information from the
     * server.
     */
    setupOutgoingCall: function() {
      var loopToken = this._conversation.get("loopToken");
      if (!loopToken) {
        this._notifier.errorL10n("missing_conversation_info");
        this.navigate("home", {trigger: true});
      } else {
        var callType = this._conversation.get("selectedCallType");

        this._conversation.once("call:outgoing", this.startCall, this);

        this._client.requestCallInfo(this._conversation.get("loopToken"),
                                     callType, function(err, sessionData) {
          if (err) {
            switch (err.errno) {
              // loop-server sends 404 + INVALID_TOKEN (errno 105) whenever a token is
              // missing OR expired; we treat this information as if the url is always
              // expired.
              case 105:
                this._onSessionExpired();
                break;
              default:
                this._notifier.errorL10n("missing_conversation_info");
                this.navigate("home", {trigger: true});
                break;
            }
            return;
          }
          this._conversation.outgoing(sessionData);
        }.bind(this));
      }
    },

    /**
     * Actually starts the call.
     */
    startCall: function() {
      var loopToken = this._conversation.get("loopToken");
      if (!loopToken) {
        this._notifier.errorL10n("missing_conversation_info");
        this.navigate("home", {trigger: true});
      } else {
        this._setupWebSocketAndCallView(loopToken);
      }
    },

    /**
     * Used to set up the web socket connection and navigate to the
     * call view if appropriate.
     *
     * @param {string} loopToken The session token to use.
     */
    _setupWebSocketAndCallView: function(loopToken) {
      this._websocket = new loop.CallConnectionWebSocket({
        url: this._conversation.get("progressURL"),
        websocketToken: this._conversation.get("websocketToken"),
        callId: this._conversation.get("callId"),
      });
      this._websocket.promiseConnect().then(function() {
        this.navigate("call/ongoing/" + loopToken, {
          trigger: true
        });
      }.bind(this), function() {
        // XXX Not the ideal response, but bug 1047410 will be replacing
        // this by better "call failed" UI.
        this._notifier.errorL10n("cannot_start_call_session_not_ready");
        return;
      }.bind(this));

      this._websocket.on("progress", this._handleWebSocketProgress, this);
    },

    /**
     * Used to receive websocket progress and to determine how to handle
     * it if appropraite.
     */
    _handleWebSocketProgress: function(progressData) {
      if (progressData.state === "terminated") {
        // XXX Before adding more states here, the basic protocol messages to the
        // server need implementing on both the standalone and desktop side.
        // These are covered by bug 1045643, but also check the dependencies on
        // bug 1034041.
        //
        // Failure to do this will break desktop - standalone call setup. We're
        // ok to handle reject, as that is a specific message from the destkop via
        // the server.
        switch (progressData.reason) {
          case "reject":
            this._handleCallRejected();
        }
      }
    },

    /**
     * Handles call rejection.
     * XXX This should really display the call failed view - bug 1046959
     * will implement this.
     */
    _handleCallRejected: function() {
      this.endCall();
      this._notifier.errorL10n("call_timeout_notification_text");
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.endCall}
     */
    endCall: function() {
      var route = "home";
      if (this._conversation.get("loopToken")) {
        route = "call/" + this._conversation.get("loopToken");
      }
      this.navigate(route, {trigger: true});
    },

    _onTimeout: function() {
      this._notifier.errorL10n("call_timeout_notification_text");
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.loadView(new HomeView());
    },

    unsupportedDevice: function() {
      this.loadView(new sharedViews.UnsupportedDeviceView());
    },

    unsupportedBrowser: function() {
      this.loadView(new sharedViews.UnsupportedBrowserView());
    },

    expired: function() {
      this.loadReactComponent(CallUrlExpiredView({helper: this.helper}));
    },

    /**
     * Loads conversation launcher view, setting the received conversation token
     * to the current conversation model. If a session is currently established,
     * terminates it first.
     *
     * @param  {String} loopToken Loop conversation token.
     */
    initiate: function(loopToken) {
      // Check if a session is ongoing; if so, terminate it
      if (this._conversation.get("ongoing")) {
        this._conversation.endSession();
      }
      this._conversation.set("loopToken", loopToken);

      var startView = StartConversationView({
        model: this._conversation,
        notifier: this._notifier,
        client: this._client
      });
      this._conversation.once("call:outgoing:setup", this.setupOutgoingCall, this);
      this.loadReactComponent(startView);
    },

    /**
     * Loads conversation establishment view.
     *
     */
    loadConversation: function(loopToken) {
      if (!this._conversation.isSessionReady()) {
        // User has loaded this url directly, actually setup the call.
        return this.navigate("call/" + loopToken, {trigger: true});
      }
      this.loadReactComponent(sharedViews.ConversationView({
        sdk: OT,
        model: this._conversation,
        video: {enabled: this._conversation.hasVideoStream("outgoing")}
      }));
    }
  });

  /**
   * Local helpers.
   */
  function WebappHelper() {
    this._iOSRegex = /^(iPad|iPhone|iPod)/;
  }

  WebappHelper.prototype = {
    isFirefox: function(platform) {
      return platform.indexOf("Firefox") !== -1;
    },

    isIOS: function(platform) {
      return this._iOSRegex.test(platform);
    }
  };

  /**
   * App initialization.
   */
  function init() {
    var helper = new WebappHelper();
    var client = new loop.StandaloneClient({
      baseServerUrl: baseServerUrl
    });
    var router = new WebappRouter({
      helper: helper,
      notifier: new sharedViews.NotificationListView({el: "#messages"}),
      client: client,
      conversation: new sharedModels.ConversationModel({}, {
        sdk: OT,
        pendingCallTimeout: loop.config.pendingCallTimeout
      })
    });

    Backbone.history.start();
    if (helper.isIOS(navigator.platform)) {
      router.navigate("unsupportedDevice", {trigger: true});
    } else if (!OT.checkSystemRequirements()) {
      router.navigate("unsupportedBrowser", {trigger: true});
    }

    document.body.classList.add(loop.shared.utils.getTargetPlatform());

    // Set the 'lang' and 'dir' attributes to <html> when the page is translated
    document.documentElement.lang = document.webL10n.getLanguage();
    document.documentElement.dir = document.webL10n.getDirection();
  }

  return {
    baseServerUrl: baseServerUrl,
    CallUrlExpiredView: CallUrlExpiredView,
    StartConversationView: StartConversationView,
    HomeView: HomeView,
    init: init,
    PromoteFirefoxView: PromoteFirefoxView,
    WebappHelper: WebappHelper,
    WebappRouter: WebappRouter
  };
})(jQuery, _, window.OT, document.webL10n);
