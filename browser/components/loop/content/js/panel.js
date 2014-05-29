/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.panel = (function(_, __) {
  "use strict";

  // XXX: baseApiUrl should be configurable (browser pref)
  var baseApiUrl = "http://localhost:5000",
      panelView;

  /**
   * Panel initialisation.
   */
  function init() {
    panelView = new PanelView();
    panelView.render();
  }

  /**
   * Notification model.
   */
  var NotificationModel = Backbone.Model.extend({
    defaults: {
      level: "info",
      message: ""
    }
  });

  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({
    model: NotificationModel
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

    dismiss: function() {
      this.$el.addClass("fade-out");
      setTimeout(function() {
        this.collection.remove(this.model);
        this.remove();
      }.bind(this), 500);
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
    initialize: function() {
      this.listenTo(this.collection, "reset add remove", this.render);
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

  /**
   * Panel view.
   */
  var PanelView = Backbone.View.extend({
    el: "#default-view",

    events: {
      "click a.get-url": "getCallUrl"
    },

    initialize: function() {
      this.client = new loop.Client({
        baseApiUrl: baseApiUrl
      });
      this.notificationCollection = new NotificationCollection();
      this.notificationListView = new NotificationListView({
        el: this.$(".messages"),
        collection: this.notificationCollection
      });
      this.notificationListView.render();
    },

    notify: function(message, level) {
      this.notificationCollection.add({
        level: level || "info",
        message: message
      });
    },

    getCallUrl: function(event) {
      event.preventDefault();
      var simplepushUrl = "http://fake.url/"; // XXX: send a real simplepush url
      this.client.requestCallUrl(simplepushUrl, function(err, callUrl) {
        if (err) {
          this.notify(__("unable_retrieve_url"), "error");
          return;
        }
        this.onCallUrlReceived(callUrl);
      }.bind(this));
    },

    onCallUrlReceived: function(callUrl) {
      this.notificationCollection.reset();
      this.$(".action .invite").hide();
      this.$(".action .result input").val(callUrl);
      this.$(".action .result").show();
      this.$(".description p").text(__("share_link_url"));
    }
  });

  return {
    init: init,
    NotificationModel: NotificationModel,
    NotificationCollection: NotificationCollection,
    NotificationView: NotificationView,
    NotificationListView: NotificationListView,
    PanelView: PanelView
  };
})(_, document.mozL10n.get);
