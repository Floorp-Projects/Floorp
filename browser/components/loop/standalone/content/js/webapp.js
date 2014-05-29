/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.webapp = (function($, TB, webl10n) {
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
   * Current conversation model instance.
   * @type {loop.shared.models.ConversationModel}
   */
  var conversation;

  /**
   * Homepage view.
   */
  var HomeView = sharedViews.BaseView.extend({
    el: "#home"
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var ConversationFormView = sharedViews.BaseView.extend({
    el: "#conversation-form",

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
      this.notifier.notify({
        message: __("unable_retrieve_call_info"),
        level: "error"
      });
    },

    /**
     * Disables this form to prevent multiple submissions.
     *
     * @see  https://bugzilla.mozilla.org/show_bug.cgi?id=991126
     */
    disableForm: function() {
      this.$("button").attr("disabled", "disabled");
    },

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
   * Webapp Router. Allows defining a main active view and easily toggling it
   * when the active route changes.
   *
   * @link http://mikeygee.com/blog/backbone.html
   */
  var WebappRouter = loop.shared.router.BaseRouter.extend({
    /**
     * Current conversation.
     * @type {loop.shared.models.ConversationModel}
     */
    _conversation: undefined,

    /**
     * Notifications dispatcher.
     * @type {loop.shared.views.NotificationListView}
     */
    _notifier: undefined,

    routes: {
      "": "home",
      "call/ongoing": "conversation",
      "call/:token": "initiate"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.conversation) {
        throw new Error("missing required conversation");
      }
      this._conversation = options.conversation;

      if (!options.notifier) {
        throw new Error("missing required notifier");
      }
      this._notifier = options.notifier;

      this.listenTo(this._conversation, "session:ready", this._onSessionReady);
      this.listenTo(this._conversation, "session:ended", this._onSessionEnded);

      // Load default view
      this.loadView(new HomeView());
    },

    /**
     * Navigates to conversation when the call session is ready.
     */
    _onSessionReady: function() {
      this.navigate("call/ongoing", {trigger: true});
    },

    /**
     * Navigates back to initiate when the call session has ended.
     */
    _onSessionEnded: function() {
      this.navigate("call/" + this._conversation.get("token"), {trigger: true});
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
    conversation: function() {
      if (!this._conversation.isSessionReady()) {
        var loopToken = this._conversation.get("loopToken");
        if (loopToken) {
          return this.navigate("call/" + loopToken, {trigger: true});
        } else {
          this._notifier.notify({
            message: __("Missing conversation information"),
            level: "error"
          });
          return this.navigate("home", {trigger: true});
        }
      }
      this.loadView(new sharedViews.ConversationView({
        sdk: TB,
        model: this._conversation
      }));
    }
  });

  /**
   * App initialization.
   */
  function init() {
    conversation = new sharedModels.ConversationModel();
    router = new WebappRouter({
      conversation: conversation,
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
})(jQuery, window.TB, document.webL10n);
