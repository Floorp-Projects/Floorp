/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */
/* jshint newcap:false */

var loop = loop || {};
loop.webapp = (function($, _, OT, mozL10n) {
  "use strict";

  loop.config = loop.config || {};
  loop.config.serverUrl = loop.config.serverUrl || "http://localhost:5000";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      baseServerUrl = loop.config.serverUrl;

  /**
   * App router.
   * @type {loop.webapp.WebappRouter}
   */
  var router;

  /**
   * Homepage view.
   */
  var HomeView = React.createClass({
    render: function() {
      return (
        <p>{mozL10n.get("welcome")}</p>
      )
    }
  });

  /**
   * Unsupported Browsers view.
   */
  var UnsupportedBrowserView = React.createClass({
    render: function() {
      var useLatestFF = mozL10n.get("use_latest_firefox", {
        "firefoxBrandNameLink": React.renderComponentToStaticMarkup(
          <a target="_blank" href="https://www.mozilla.org/firefox/">Firefox</a>
        )
      });
      return (
        <div>
          <h2>{mozL10n.get("incompatible_browser")}</h2>
          <p>{mozL10n.get("powered_by_webrtc")}</p>
          <p dangerouslySetInnerHTML={{__html: useLatestFF}}></p>
        </div>
      );
    }
  });

  /**
   * Unsupported Device view.
   */
  var UnsupportedDeviceView = React.createClass({
    render: function() {
      return (
        <div>
          <h2>{mozL10n.get("incompatible_device")}</h2>
          <p>{mozL10n.get("sorry_device_unsupported")}</p>
          <p>{mozL10n.get("use_firefox_windows_mac_linux")}</p>
        </div>
      );
    }
  });

  /**
   * Firefox promotion interstitial. Will display only to non-Firefox users.
   */
  var PromoteFirefoxView = React.createClass({
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      if (this.props.helper.isFirefox(navigator.userAgent)) {
        return <div />;
      }
      return (
        <div className="promote-firefox">
          <h3>{mozL10n.get("promote_firefox_hello_heading")}</h3>
          <p>
            <a className="btn btn-large btn-accept"
               href="https://www.mozilla.org/firefox/">
              {mozL10n.get("get_firefox_button")}
            </a>
          </p>
        </div>
      );
    }
  });

  /**
   * Expired call URL view.
   */
  var CallUrlExpiredView = React.createClass({
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      /* jshint ignore:start */
      return (
        <div className="expired-url-info">
          <div className="info-panel">
            <div className="firefox-logo" />
            <h1>{mozL10n.get("call_url_unavailable_notification_heading")}</h1>
            <h4>{mozL10n.get("call_url_unavailable_notification_message2")}</h4>
          </div>
          <PromoteFirefoxView helper={this.props.helper} />
        </div>
      );
      /* jshint ignore:end */
    }
  });

  var ConversationBranding = React.createClass({
    render: function() {
      return (
        <h1 className="standalone-header-title">
          <strong>{mozL10n.get("brandShortname")}</strong> {mozL10n.get("clientShortname")}
        </h1>
      );
    }
  });

  var ConversationHeader = React.createClass({
    render: function() {
      var cx = React.addons.classSet;
      var conversationUrl = location.href;

      var urlCreationDateClasses = cx({
        "light-color-font": true,
        "call-url-date": true, /* Used as a handler in the tests */
        /*hidden until date is available*/
        "hide": !this.props.urlCreationDateString.length
      });

      var callUrlCreationDateString = mozL10n.get("call_url_creation_date_label", {
        "call_url_creation_date": this.props.urlCreationDateString
      });

      return (
        /* jshint ignore:start */
        <header className="standalone-header header-box container-box">
          <ConversationBranding />
          <div className="loop-logo" title="Firefox WebRTC! logo"></div>
          <h3 className="call-url">
            {conversationUrl}
          </h3>
          <h4 className={urlCreationDateClasses} >
            {callUrlCreationDateString}
          </h4>
        </header>
        /* jshint ignore:end */
      );
    }
  });

  var ConversationFooter = React.createClass({
    render: function() {
      return (
        <div className="standalone-footer container-box">
          <div title="Mozilla Logo" className="footer-logo"></div>
        </div>
      );
    }
  });

  var PendingConversationView = React.createClass({
    getInitialState: function() {
      return {
        callState: this.props.callState || "connecting"
      }
    },

    propTypes: {
      websocket: React.PropTypes.instanceOf(loop.CallConnectionWebSocket)
                      .isRequired
    },

    componentDidMount: function() {
      this.props.websocket.listenTo(this.props.websocket, "progress:alerting",
                                    this._handleRingingProgress);
    },

    _handleRingingProgress: function() {
      this.setState({callState: "ringing"});
    },

    _cancelOutgoingCall: function() {
      this.props.websocket.cancel();
    },

    render: function() {
      var callState = mozL10n.get("call_progress_" + this.state.callState + "_description");
      return (
        /* jshint ignore:start */
        <div className="container">
          <div className="container-box">
            <header className="pending-header header-box">
              <ConversationBranding />
            </header>

            <div id="cameraPreview"></div>

            <div id="messages"></div>

            <p className="standalone-btn-label">
              {callState}
            </p>

            <div className="btn-pending-cancel-group btn-group">
              <div className="flex-padding-1"></div>
              <button className="btn btn-large btn-cancel"
                      onClick={this._cancelOutgoingCall} >
                <span className="standalone-call-btn-text">
                  {mozL10n.get("initiate_call_cancel_button")}
                </span>
              </button>
              <div className="flex-padding-1"></div>
            </div>
          </div>

          <ConversationFooter />
        </div>
        /* jshint ignore:end */
      );
    }
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var StartConversationView = React.createClass({
    /**
     * Constructor.
     *
     * Required options:
     * - {loop.shared.models.ConversationModel}    model    Conversation model.
     * - {loop.shared.models.NotificationCollection} notifications
     *
     */

    getInitialProps: function() {
      return {showCallOptionsMenu: false};
    },

    getInitialState: function() {
      return {
        urlCreationDateString: '',
        disableCallButton: false,
        showCallOptionsMenu: this.props.showCallOptionsMenu
      };
    },

    propTypes: {
      model: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                                       .isRequired,
      // XXX Check more tightly here when we start injecting window.loop.*
      notifications: React.PropTypes.object.isRequired,
      client: React.PropTypes.object.isRequired
    },

    componentDidMount: function() {
      // Listen for events & hide dropdown menu if user clicks away
      window.addEventListener("click", this.clickHandler);
      this.props.model.listenTo(this.props.model, "session:error",
                                this._onSessionError);
      this.props.client.requestCallUrlInfo(this.props.model.get("loopToken"),
                                           this._setConversationTimestamp);
    },

    _onSessionError: function(error) {
      console.error(error);
      this.props.notifications.errorL10n("unable_retrieve_call_info");
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
        this.props.notifications.errorL10n("unable_retrieve_call_info");
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
      var tos_link_name = mozL10n.get("terms_of_use_link_text");
      var privacy_notice_name = mozL10n.get("privacy_notice_link_text");

      var tosHTML = mozL10n.get("legal_text_and_links", {
        "terms_of_use_url": "<a target=_blank href='" +
          "https://accounts.firefox.com/legal/terms'>" + tos_link_name + "</a>",
        "privacy_notice_url": "<a target=_blank href='" +
          "https://www.mozilla.org/privacy/'>" + privacy_notice_name + "</a>"
      });

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
        <div className="container">
          <div className="container-box">

            <ConversationHeader
              urlCreationDateString={this.state.urlCreationDateString} />

            <p className="standalone-btn-label">
              {mozL10n.get("initiate_call_button_label2")}
            </p>

            <div id="messages"></div>

            <div className="btn-group">
              <div className="flex-padding-1"></div>
              <div className="standalone-btn-chevron-menu-group">
                <div className="btn-group-chevron">
                  <div className="btn-group">

                    <button className="btn btn-large btn-accept"
                            onClick={this._initiateOutgoingCall("audio-video")}
                            disabled={this.state.disableCallButton}
                            title={mozL10n.get("initiate_audio_video_call_tooltip2")} >
                      <span className="standalone-call-btn-text">
                        {mozL10n.get("initiate_audio_video_call_button2")}
                      </span>
                      <span className="standalone-call-btn-video-icon"></span>
                    </button>

                    <div className="btn-chevron"
                         onClick={this._toggleCallOptionsMenu}>
                    </div>

                  </div>

                  <ul className={dropdownMenuClasses}>
                    <li>
                      {/*
                       Button required for disabled state.
                       */}
                      <button className="start-audio-only-call"
                              onClick={this._initiateOutgoingCall("audio")}
                              disabled={this.state.disableCallButton} >
                        {mozL10n.get("initiate_audio_call_button2")}
                      </button>
                    </li>
                  </ul>

                </div>
              </div>
              <div className="flex-padding-1"></div>
            </div>

            <p className={tosClasses}
               dangerouslySetInnerHTML={{__html: tosHTML}}></p>
          </div>

          <ConversationFooter />
        </div>
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
      "call/pending/:token": "pendingConversation",
      "call/ongoing/:token": "loadConversation",
      "call/:token":         "initiate"
    },

    initialize: function(options) {
      this.helper = options.helper;
      if (!this.helper) {
        throw new Error("WebappRouter requires a helper object");
      }

      // Load default view
      this.loadReactComponent(<HomeView />);
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
        this._notifications.errorL10n("missing_conversation_info");
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
                this._notifications.errorL10n("missing_conversation_info");
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
        this._notifications.errorL10n("missing_conversation_info");
        this.navigate("home", {trigger: true});
      } else {
        this.navigate("call/pending/" + loopToken, {
          trigger: true
        });
      }
    },

    /**
     * Used to set up the web socket connection and navigate to the
     * call view if appropriate.
     *
     * @param {string} loopToken The session token to use.
     */
    _setupWebSocketAndCallView: function() {
      this._websocket = new loop.CallConnectionWebSocket({
        url: this._conversation.get("progressURL"),
        websocketToken: this._conversation.get("websocketToken"),
        callId: this._conversation.get("callId"),
      });
      this._websocket.promiseConnect().then(function() {
      }.bind(this), function() {
        // XXX Not the ideal response, but bug 1047410 will be replacing
        // this by better "call failed" UI.
        this._notifications.errorL10n("cannot_start_call_session_not_ready");
        return;
      }.bind(this));

      this._websocket.on("progress", this._handleWebSocketProgress, this);
    },

    /**
     * Checks if the streams have been connected, and notifies the
     * websocket that the media is now connected.
     */
    _checkConnected: function() {
      // Check we've had both local and remote streams connected before
      // sending the media up message.
      if (this._conversation.streamsConnected()) {
        this._websocket.mediaUp();
      }
    },

    /**
     * Used to receive websocket progress and to determine how to handle
     * it if appropraite.
     */
    _handleWebSocketProgress: function(progressData) {
      switch(progressData.state) {
        case "connecting": {
          this._handleCallConnecting();
          break;
        }
        case "terminated": {
          // At the moment, we show the same text regardless
          // of the terminated reason.
          this._handleCallTerminated(progressData.reason);
          break;
        }
      }
    },

    /**
     * Handles a call moving to the connecting stage.
     */
    _handleCallConnecting: function() {
      var loopToken = this._conversation.get("loopToken");
      if (!loopToken) {
        this._notifications.errorL10n("missing_conversation_info");
        return;
      }

      this.navigate("call/ongoing/" + loopToken, {
        trigger: true
      });
    },

    /**
     * Handles call rejection.
     *
     * @param {String} reason The reason the call was terminated.
     */
    _handleCallTerminated: function(reason) {
      this.endCall();
      // For reasons other than cancel, display some notification text.
      if (reason !== "cancel") {
        // XXX This should really display the call failed view - bug 1046959
        // will implement this.
        this._notifications.errorL10n("call_timeout_notification_text");
      }
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

    /**
     * Default entry point.
     */
    home: function() {
      this.loadReactComponent(<HomeView />);
    },

    unsupportedDevice: function() {
      this.loadReactComponent(<UnsupportedDeviceView />);
    },

    unsupportedBrowser: function() {
      this.loadReactComponent(<UnsupportedBrowserView />);
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
        notifications: this._notifications,
        client: this._client
      });
      this._conversation.once("call:outgoing:setup", this.setupOutgoingCall, this);
      this._conversation.once("change:publishedStream", this._checkConnected, this);
      this._conversation.once("change:subscribedStream", this._checkConnected, this);
      this.loadReactComponent(startView);
    },

    pendingConversation: function(loopToken) {
      if (!this._conversation.isSessionReady()) {
        // User has loaded this url directly, actually setup the call.
        return this.navigate("call/" + loopToken, {trigger: true});
      }
      this._setupWebSocketAndCallView();
      this.loadReactComponent(PendingConversationView({
        websocket: this._websocket
      }));
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
      notifications: new sharedModels.NotificationCollection(),
      client: client,
      conversation: new sharedModels.ConversationModel({}, {
        sdk: OT
      })
    });

    Backbone.history.start();
    if (helper.isIOS(navigator.platform)) {
      router.navigate("unsupportedDevice", {trigger: true});
    } else if (!OT.checkSystemRequirements()) {
      router.navigate("unsupportedBrowser", {trigger: true});
    }

    // Set the 'lang' and 'dir' attributes to <html> when the page is translated
    document.documentElement.lang = mozL10n.language.code;
    document.documentElement.dir = mozL10n.language.direction;
  }

  return {
    baseServerUrl: baseServerUrl,
    CallUrlExpiredView: CallUrlExpiredView,
    PendingConversationView: PendingConversationView,
    StartConversationView: StartConversationView,
    HomeView: HomeView,
    UnsupportedBrowserView: UnsupportedBrowserView,
    UnsupportedDeviceView: UnsupportedDeviceView,
    init: init,
    PromoteFirefoxView: PromoteFirefoxView,
    WebappHelper: WebappHelper,
    WebappRouter: WebappRouter
  };
})(jQuery, _, window.OT, navigator.mozL10n);
