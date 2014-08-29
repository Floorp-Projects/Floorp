/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.webapp = (function($, _, OT) {
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
  var HomeView = sharedViews.BaseView.extend({
    template: _.template('<p data-l10n-id="welcome"></p>')
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var StartConversationView = sharedViews.BaseView.extend({
    template: _.template([
      '<form>',
      '  <p>',
      '    <button class="btn btn-success" data-l10n-id="start_call"></button>',
      '  </p>',
      '</form>'
    ].join("")),

    events: {
      "submit": "initiateOutgoingCall"
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
    initiateOutgoingCall: function(event) {
      event.preventDefault();
      this.model.setupOutgoingCall();
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
      "call/ongoing/:token": "loadConversation",
      "call/:token":         "initiate"
    },

    initialize: function() {
      // Load default view
      this.loadView(new HomeView());

      this.listenTo(this._conversation, "timeout", this._onTimeout);
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
        this._conversation.once("call:outgoing", this.startCall, this);

        // XXX For now, we assume both audio and video as there is no
        // other option to select (bug 1048333)
        this._client.requestCallInfo(this._conversation.get("loopToken"), "audio-video",
                                     (err, sessionData) => {
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
        });
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

      var startView = new StartConversationView({
        model: this._conversation,
        notifier: this._notifier,
        client: this._client
      });
      this._conversation.once("call:outgoing:setup", this.setupOutgoingCall, this);
      this._conversation.once("change:publishedStream", this._checkConnected, this);
      this._conversation.once("change:subscribedStream", this._checkConnected, this);
      this.loadView(startView);
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

  WebappHelper.prototype.isIOS = function isIOS(platform) {
    return this._iOSRegex.test(platform);
  };

  /**
   * App initialization.
   */
  function init() {
    var helper = new WebappHelper();
    var client = new loop.StandaloneClient({
      baseServerUrl: baseServerUrl
    });
    router = new WebappRouter({
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
  }

  return {
    baseServerUrl: baseServerUrl,
    StartConversationView: StartConversationView,
    HomeView: HomeView,
    WebappHelper: WebappHelper,
    init: init,
    WebappRouter: WebappRouter
  };
})(jQuery, _, window.OT);
