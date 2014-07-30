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
            React.DOM.a({className: "btn btn-large btn-success", 
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

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var ConversationFormView = sharedViews.BaseView.extend({
    template: _.template([
      '<form>',
      '  <p>',
      '    <button class="btn btn-success" data-l10n-id="start_call"></button>',
      '  </p>',
      '</form>'
    ].join("")),

    events: {
      "submit": "initiate"
    },

    /**
     * Constructor.
     *
     * Required options:
     * - {loop.shared.model.ConversationModel}    model    Conversation model.
     * - {loop.shared.views.NotificationListView} notifier Notifier component.
     *
     * @param  {Object} options Options object.
     */
    initialize: function(options) {
      options = options || {};

      if (!options.model) {
        throw new Error("missing required model");
      }
      this.model = options.model;

      if (!options.notifier) {
        throw new Error("missing required notifier");
      }
      this.notifier = options.notifier;

      this.listenTo(this.model, "session:error", this._onSessionError);
    },

    _onSessionError: function(error) {
      console.error(error);
      this.notifier.errorL10n("unable_retrieve_call_info");
    },

    /**
     * Disables this form to prevent multiple submissions.
     *
     * @see  https://bugzilla.mozilla.org/show_bug.cgi?id=991126
     */
    disableForm: function() {
      this.$("button").attr("disabled", "disabled");
    },

    /**
     * Initiates the call.
     *
     * @param {SubmitEvent} event
     */
    initiate: function(event) {
      event.preventDefault();
      this.model.initiate({
        client: new loop.StandaloneClient({
          baseServerUrl: baseServerUrl
        }),
        outgoing: true,
        // For now, we assume both audio and video as there is no
        // other option to select.
        callType: "audio-video"
      });
      this.disableForm();
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
        throw new Error("WebappRouter requires an helper object");
      }

      // Load default view
      this.loadView(new HomeView());

      this.listenTo(this._conversation, "timeout", this._onTimeout);
      this.listenTo(this._conversation, "session:expired",
                    this._onSessionExpired);
    },

    _onSessionExpired: function() {
      this.navigate("/call/expired", {trigger: true});
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.startCall}
     */
    startCall: function() {
      if (!this._conversation.get("loopToken")) {
        this._notifier.errorL10n("missing_conversation_info");
        this.navigate("home", {trigger: true});
      } else {
        this.navigate("call/ongoing/" + this._conversation.get("loopToken"), {
          trigger: true
        });
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
      this.loadView(new ConversationFormView({
        model: this._conversation,
        notifier: this._notifier
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
        model: this._conversation
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
    router = new WebappRouter({
      helper: helper,
      notifier: new sharedViews.NotificationListView({el: "#messages"}),
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
  }

  return {
    baseServerUrl: baseServerUrl,
    CallUrlExpiredView: CallUrlExpiredView,
    ConversationFormView: ConversationFormView,
    HomeView: HomeView,
    init: init,
    PromoteFirefoxView: PromoteFirefoxView,
    WebappHelper: WebappHelper,
    WebappRouter: WebappRouter
  };
})(jQuery, _, window.OT, document.webL10n);
