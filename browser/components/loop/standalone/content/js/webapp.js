/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.webapp = (function($, OT, webl10n) {
  "use strict";

  /**
   * Base Loop server URL.
   *
   * XXX: should be configurable, but how?
   *
   * @type {String}
   */
  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      // XXX this one should be configurable
      //     see https://bugzilla.mozilla.org/show_bug.cgi?id=987086
      baseServerUrl = "http://localhost:5000",
      // aliasing translation function as __ for concision
      __ = webl10n.get;

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
      this.notifier.error(__("unable_retrieve_call_info"));
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
        baseServerUrl: baseServerUrl,
        outgoing: true
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
      "call/ongoing/:token": "conversation",
      "call/:token":         "initiate"
    },

    initialize: function() {
      // Load default view
      this.loadView(new HomeView());
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.startCall}
     */
    startCall: function() {
      var route;
      if (this._conversation.get("loopToken")) {
        route = "call/ongoing/" + this._conversation.get("loopToken");
      } else {
        route = "home";
        this._notifier.error(__("missing_conversation_info"));
      }
      this.navigate(route, {trigger: true});
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
      this.loadView(new HomeView());
    },

    /**
     * Loads conversation launcher view, setting the received conversation token
     * to the current conversation model.
     *
     * @param  {String} loopToken Loop conversation token.
     */
    initiate: function(loopToken) {
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
    conversation: function(loopToken) {
      if (!this._conversation.isSessionReady()) {
        // User has loaded this url directly, need to actually setup the call
        return this.navigate("call/" + loopToken, {trigger: true});
      }
      this.loadView(new sharedViews.ConversationView({
        sdk: OT,
        model: this._conversation
      }));
    }
  });

  /**
   * App initialization.
   */
  function init() {
    router = new WebappRouter({
      conversation: new sharedModels.ConversationModel(),
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();
  }

  return {
    baseServerUrl: baseServerUrl,
    ConversationFormView: ConversationFormView,
    HomeView: HomeView,
    init: init,
    WebappRouter: WebappRouter
  };
})(jQuery, window.OT, document.webL10n);
