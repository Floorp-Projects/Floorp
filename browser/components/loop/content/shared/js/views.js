/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(_, OT, l10n) {
  "use strict";

  var sharedModels = loop.shared.models;

  /**
   * L10n view. Translates resulting view DOM fragment once rendered.
   */
  var L10nView = (function() {
    var L10nViewImpl   = Backbone.View.extend(), // Original View constructor
        originalExtend = L10nViewImpl.extend;    // Original static extend fn

    /**
     * Patches View extend() method so we can hook and patch any declared render
     * method.
     *
     * @return {Backbone.View} Extended view with patched render() method.
     */
    L10nViewImpl.extend = function() {
      var ExtendedView   = originalExtend.apply(this, arguments),
          originalRender = ExtendedView.prototype.render;

      /**
       * Wraps original render() method to translate contents once they're
       * rendered.
       *
       * @return {Backbone.View} Extended view instance.
       */
      ExtendedView.prototype.render = function() {
        if (originalRender) {
          originalRender.apply(this, arguments);
          l10n.translate(this.el);
        }
        return this;
      };

      return ExtendedView;
    };

    return L10nViewImpl;
  })();

  /**
   * Base view.
   */
  var BaseView = L10nView.extend({
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
    },

    /**
     * Base render implementation: renders an attached template if available.
     *
     * Note: You need to override this if you want to do fancier stuff, eg.
     *       rendering the template using model data.
     *
     * @return {BaseView}
     */
    render: function() {
      if (this.template) {
        this.$el.html(this.template());
      }
      return this;
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = BaseView.extend({
    className: "conversation",

    template: _.template([
      '<nav class="controls">',
      '  <button class="btn stop" data-l10n-id="stop"></button>',
      '</nav>',
      '<div class="media nested">',
      // Both these wrappers are required by the SDK; this is fragile and
      // will break if a future version of the SDK updates this generated DOM,
      // especially as the SDK seems to actually move wrapped contents into
      // their own generated stuff.
      '  <div class="remote"><div class="incoming"></div></div>',
      '  <div class="local"><div class="outgoing"></div></div>',
      '</div>'
    ].join("")),

    // height set to "auto" to fix video layout on Google Chrome
    // @see https://bugzilla.mozilla.org/show_bug.cgi?id=991122
    videoStyles: {
      width: "100%",
      height: "auto",
      style: { "bugDisplayMode": "off" }
    },

    events: {
      'click .btn.stop': 'hangup'
    },

    /**
     * Establishes webrtc communication using OT sdk.
     */
    initialize: function(options) {
      options = options || {};
      if (!options.sdk) {
        throw new Error("missing required sdk");
      }
      this.sdk = options.sdk;

      this.listenTo(this.model, "session:connected", this.publish);
      this.listenTo(this.model, "session:stream-created", this._streamCreated);
      this.listenTo(this.model, ["session:peer-hungup",
                                 "session:network-disconnected",
                                 "session:ended"].join(" "), this.unpublish);
      this.model.startSession();
    },

    /**
     * Subscribes and attaches each created stream to a DOM element.
     *
     * XXX: for now we only support a single remote stream, hence a single DOM
     *      element.
     *
     * http://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     *
     * @param  {StreamEvent} event
     */
    _streamCreated: function(event) {
      var incoming = this.$(".incoming").get(0);
      event.streams.forEach(function(stream) {
        if (stream.connection.connectionId !==
            this.model.session.connection.connectionId) {
          this.model.session.subscribe(stream, incoming, this.videoStyles);
        }
      }.bind(this));
    },

    /**
     * Hangs up current conversation.
     *
     * @param  {MouseEvent} event
     */
    hangup: function(event) {
      event.preventDefault();
      this.unpublish();
      this.model.endSession();
    },

    /**
     * Publishes remote streams available once a session is connected.
     *
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionConnectEvent.html
     *
     * @param  {SessionConnectEvent} event
     */
    publish: function(event) {
      var outgoing = this.$(".outgoing").get(0);

      this.publisher = this.sdk.initPublisher(outgoing, this.videoStyles);

      // Suppress OT GuM custom dialog, see bug 1018875
      function preventOpeningAccessDialog(event) {
        event.preventDefault();
      }
      this.publisher.on("accessDialogOpened", preventOpeningAccessDialog);
      this.publisher.on("accessDenied", preventOpeningAccessDialog);

      this.model.session.publish(this.publisher);
    },

    /**
     * Unpublishes local stream.
     */
    unpublish: function() {
      // Unregister access OT GuM custom dialog listeners, see bug 1018875
      this.publisher.off("accessDialogOpened");
      this.publisher.off("accessDenied");

      this.model.session.unpublish(this.publisher);
    },

    /**
     * Renders this view.
     *
     * @return {ConversationView}
     */
    render: function() {
      this.$el.html(this.template(this.model.toJSON()));
      return this;
    }
  });

  /**
   * Notification view.
   */
  var NotificationView = BaseView.extend({
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

    /**
     * Adds a new notification to the stack using an l10n message identifier,
     * triggering rendering of it.
     *
     * @param  {String} messageId L10n message id
     * @param  {String} level     Notification level
     */
    notifyL10n: function(messageId, level) {
      this.notify({
        message: l10n.get(messageId),
        level: level
      });
    },

    /**
     * Adds a warning notification to the stack and renders it.
     *
     * @return {String} message
     */
    warn: function(message) {
      this.notify({level: "warning", message: message});
    },

    /**
     * Adds a l10n warning notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     */
    warnL10n: function(messageId) {
      this.warn(l10n.get(messageId));
    },

    /**
     * Adds an error notification to the stack and renders it.
     *
     * @return {String} message
     */
    error: function(message) {
      this.notify({level: "error", message: message});
    },

    /**
     * Adds a l10n rror notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     */
    errorL10n: function(messageId) {
      this.error(l10n.get(messageId));
    },

    /**
     * Renders this view.
     *
     * @return {loop.shared.views.NotificationListView}
     */
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
   * Unsupported Browsers view.
   */
  var UnsupportedBrowserView = BaseView.extend({
    template: _.template([
      '<div>',
      '  <h2 data-l10n-id="incompatible_browser"></h2>',
      '  <p data-l10n-id="powered_by_webrtc"></p>',
      '  <p data-l10n-id="use_latest_firefox" ',
      '    data-l10n-args=\'{"ff_url": "https://www.mozilla.org/firefox/"}\'>',
      '  </p>',
      '</div>'
    ].join(""))
  });

  /**
   * Unsupported Browsers view.
   */
  var UnsupportedDeviceView = BaseView.extend({
    template: _.template([
      '<div>',
      '  <h2 data-l10n-id="incompatible_device"></h2>',
      '  <p data-l10n-id="sorry_device_unsupported"></p>',
      '  <p data-l10n-id="use_firefox_windows_mac_linux"></p>',
      '</div>'
    ].join(""))
  });

  return {
    L10nView: L10nView,
    BaseView: BaseView,
    ConversationView: ConversationView,
    NotificationListView: NotificationListView,
    NotificationView: NotificationView,
    UnsupportedBrowserView: UnsupportedBrowserView,
    UnsupportedDeviceView: UnsupportedDeviceView
  };
})(_, window.OT, document.webL10n || document.mozL10n);
