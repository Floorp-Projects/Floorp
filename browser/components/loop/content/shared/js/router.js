/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.router = (function(l10n) {
  "use strict";

  /**
   * Base Router. Allows defining a main active view and ease toggling it when
   * the active route changes.
   *
   * @link http://mikeygee.com/blog/backbone.html
   */
  var BaseRouter = Backbone.Router.extend({
    /**
     * Active view.
     * @type {Object}
     */
    _activeView: undefined,

    /**
     * Notifications dispatcher.
     * @type {loop.shared.views.NotificationListView}
     */
    _notifier: undefined,

    /**
     * Constructor.
     *
     * Required options:
     * - {loop.shared.views.NotificationListView} notifier Notifier view.
     *
     * @param  {Object} options Options object.
     */
    constructor: function(options) {
      options = options || {};
      if (!options.notifier) {
        throw new Error("missing required notifier");
      }
      this._notifier = options.notifier;

      Backbone.Router.apply(this, arguments);
    },

    /**
     * Loads and render current active view.
     *
     * @param {loop.shared.views.BaseView} view View.
     */
    loadView: function(view) {
      this.clearActiveView();
      this._activeView = {type: "backbone", view: view.render().show()};
      this.updateView(this._activeView.view.$el);
    },

    /**
     * Renders a React component as current active view.
     *
     * @param {React} reactComponent React component.
     */
    loadReactComponent: function(reactComponent) {
      this.clearActiveView();
      this._activeView = {
        type: "react",
        view: React.renderComponent(reactComponent,
                                    document.querySelector("#main"))
      };
    },

    /**
     * Clears current active view.
     */
    clearActiveView: function() {
      if (!this._activeView) {
        return;
      }
      if (this._activeView.type === "react") {
        React.unmountComponentAtNode(document.querySelector("#main"));
      } else {
        this._activeView.view.remove();
      }
    },

    /**
     * Updates main div element with provided contents.
     *
     * @param  {jQuery} $el Element.
     */
    updateView: function($el) {
      $("#main").html($el);
    }
  });

  /**
   * Base conversation router, implementing common behaviors when handling
   * a conversation.
   */
  var BaseConversationRouter = BaseRouter.extend({
    /**
     * Current conversation.
     * @type {loop.shared.models.ConversationModel}
     */
    _conversation: undefined,

    /**
     * Constructor. Defining it as `constructor` allows implementing an
     * `initialize` method in child classes without needing calling this parent
     * one. See http://backbonejs.org/#Model-constructor (same for Router)
     *
     * Required options:
     * - {loop.shared.model.ConversationModel}    model    Conversation model.
     *
     * @param {Object} options Options object.
     */
    constructor: function(options) {
      options = options || {};
      if (!options.conversation) {
        throw new Error("missing required conversation");
      }
      this._conversation = options.conversation;

      this.listenTo(this._conversation, "session:ready", this._onSessionReady);
      this.listenTo(this._conversation, "session:ended", this._onSessionEnded);
      this.listenTo(this._conversation, "session:peer-hungup",
                                        this._onPeerHungup);
      this.listenTo(this._conversation, "session:network-disconnected",
                                        this._onNetworkDisconnected);
      this.listenTo(this._conversation, "session:connection-error",
                    this._notifyError);

      BaseRouter.apply(this, arguments);
    },

    /**
     * Notify the user that the connection was not possible
     * @param {{code: number, message: string}} error
     */
    _notifyError: function(error) {
      console.log(error);
      this._notifier.errorL10n("connection_error_see_console_notification");
      this.endCall();
    },

    /**
     * Starts the call. This method should be overriden.
     */
    startCall: function() {},

    /**
     * Ends the call. This method should be overriden.
     */
    endCall: function() {},

    /**
     * Session is ready.
     */
    _onSessionReady: function() {
      this.startCall();
    },

    /**
     * Session has ended. Notifies the user and ends the call.
     */
    _onSessionEnded: function() {
      this._notifier.warnL10n("call_has_ended");
      this.endCall();
    },

    /**
     * Peer hung up. Notifies the user and ends the call.
     *
     * Event properties:
     * - {String} connectionId: OT session id
     *
     * @param {Object} event
     */
    _onPeerHungup: function() {
      this._notifier.warnL10n("peer_ended_conversation");
      this.endCall();
    },

    /**
     * Network disconnected. Notifies the user and ends the call.
     */
    _onNetworkDisconnected: function() {
      this._notifier.warnL10n("network_disconnected");
      this.endCall();
    }
  });

  return {
    BaseRouter: BaseRouter,
    BaseConversationRouter: BaseConversationRouter
  };
})(document.webL10n || document.mozL10n);
