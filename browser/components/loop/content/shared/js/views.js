/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(TB) {
  "use strict";

  var sharedModels = loop.shared.models;

  /**
   * Base Backbone view.
   */
  var BaseView = Backbone.View.extend({
    /**
     * Hides view element.
     *
     * @return {BaseView}
     */
    hide: function() {
      this.$el.hide();
      return this;
    },

    /**
     * Shows view element.
     *
     * @return {BaseView}
     */
    show: function() {
      this.$el.show();
      return this;
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = BaseView.extend({
    el: "#conversation",

    // height set to "auto" to fix video layout on Google Chrome
    // @see https://bugzilla.mozilla.org/show_bug.cgi?id=991122
    videoStyles: { width: "100%", height: "auto" },

    /**
     * Establishes webrtc communication using TB sdk.
     */
    initialize: function(options) {
      options = options || {};
      if (!options.sdk) {
        throw new Error("missing required sdk");
      }
      this.sdk = options.sdk;
      // XXX: this feels like to be moved to the ConversationModel, but as it's
      // tighly coupled with the DOM (element ids to receive streams), we'd need
      // an abstraction we probably don't want yet.
      this.session   = this.sdk.initSession(this.model.get("sessionId"));
      this.publisher = this.sdk.initPublisher(this.model.get("apiKey"),
                                              "outgoing", this.videoStyles);

      this.session.connect(this.model.get("apiKey"),
                           this.model.get("sessionToken"));

      this.listenTo(this.session, "sessionConnected", this._sessionConnected);
      this.listenTo(this.session, "streamCreated", this._streamCreated);
      this.listenTo(this.session, "connectionDestroyed", this._sessionEnded);
    },

    _sessionConnected: function(event) {
      this.session.publish(this.publisher);
      this._subscribeToStreams(event.streams);
    },

    _streamCreated: function(event) {
      this._subscribeToStreams(event.streams);
    },

    _sessionEnded: function(event) {
      // XXX: better end user notification
      alert("Your session has ended. Reason: " + event.reason);
      this.model.trigger("session:ended");
    },

    _subscribeToStreams: function(streams) {
      streams.forEach(function(stream) {
        if (stream.connection.connectionId !==
            this.session.connection.connectionId) {
          this.session.subscribe(stream, "incoming", this.videoStyles);
        }
      }.bind(this));
    }
  });

  /**
   * Notification view.
   */
  var NotificationView = Backbone.View.extend({
    template: _.template([
      '<div class="alert alert-<%- level %>">',
      '  <button class="close"></button>',
      '  <p class="message"><%- message %></p>',
      '</div>'
    ].join("")),

    events: {
      "click .close": "dismiss"
    },

    dismiss: function(event) {
      event.preventDefault();
      this.$el.addClass("fade-out");
      setTimeout(function() {
        this.collection.remove(this.model);
        this.remove();
      }.bind(this), 500); // XXX make timeout value configurable
    },

    render: function() {
      this.$el.html(this.template(this.model.toJSON()));
      return this;
    }
  });

  /**
   * Notification list view.
   */
  var NotificationListView = Backbone.View.extend({
    /**
     * Constructor.
     *
     * Available options:
     * - {loop.shared.models.NotificationCollection} collection Notifications
     *                                                          collection
     *
     * @param  {Object} options Options object
     */
    initialize: function(options) {
      options = options || {};
      if (!options.collection) {
        this.collection = new sharedModels.NotificationCollection();
      }
      this.listenTo(this.collection, "reset add remove", this.render);
    },

    /**
     * Clears the notification stack.
     */
    clear: function() {
      this.collection.reset();
    },

    /**
     * Adds a new notification to the stack, triggering rendering of it.
     *
     * @param  {Object|NotificationModel} notification Notification data.
     */
    notify: function(notification) {
      this.collection.add(notification);
    },

    render: function() {
      this.$el.html(this.collection.map(function(notification) {
        return new NotificationView({
          model: notification,
          collection: this.collection
        }).render().$el;
      }.bind(this)));
      return this;
    }
  });

  return {
    BaseView: BaseView,
    ConversationView: ConversationView,
    NotificationListView: NotificationListView,
    NotificationView: NotificationView
  };
})(window.TB);
