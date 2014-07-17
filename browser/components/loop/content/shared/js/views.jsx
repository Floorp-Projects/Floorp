/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */
var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(_, OT, l10n) {
  "use strict";

  var sharedModels = loop.shared.models;
  var __ = l10n.get;

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
   * Media control button.
   *
   * Required props:
   * - {String}   scope   Media scope, can be "local" or "remote".
   * - {String}   type    Media type, can be "audio" or "video".
   * - {Function} action  Function to be executed on click.
   * - {Enabled}  enabled Stream activation status (default: true).
   */
  var MediaControlButton = React.createClass({
    propTypes: {
      scope: React.PropTypes.string.isRequired,
      type: React.PropTypes.string.isRequired,
      action: React.PropTypes.func.isRequired,
      enabled: React.PropTypes.bool.isRequired
    },

    getDefaultProps: function() {
      return {enabled: true};
    },

    handleClick: function() {
      this.props.action();
    },

    _getClasses: function() {
      var cx = React.addons.classSet;
      // classes
      var classesObj = {
        "btn": true,
        "media-control": true,
        "local-media": this.props.scope === "local",
        "muted": !this.props.enabled
      };
      classesObj["btn-mute-" + this.props.type] = true;
      return cx(classesObj);
    },

    _getTitle: function(enabled) {
      var prefix = this.props.enabled ? "mute" : "unmute";
      var suffix = "button_title";
      var msgId = [prefix, this.props.scope, this.props.type, suffix].join("_");
      return __(msgId);
    },

    render: function() {
      return (
        <button className={this._getClasses()}
                title={this._getTitle()}
                onClick={this.handleClick}></button>
      );
    }
  });

  /**
   * Conversation controls.
   */
  var ConversationToolbar = React.createClass({
    getDefaultProps: function() {
      return {
        video: {enabled: true},
        audio: {enabled: true}
      };
    },

    propTypes: {
      video: React.PropTypes.object.isRequired,
      audio: React.PropTypes.object.isRequired,
      hangup: React.PropTypes.func.isRequired,
      publishStream: React.PropTypes.func.isRequired
    },

    handleClickHangup: function() {
      this.props.hangup();
    },

    handleToggleVideo: function() {
      this.props.publishStream("video", !this.props.video.enabled);
    },

    handleToggleAudio: function() {
      this.props.publishStream("audio", !this.props.audio.enabled);
    },

    render: function() {
      return (
        <ul className="controls">
          <li><button className="btn btn-hangup"
                      onClick={this.handleClickHangup}
                      title={__("hangup_button_title")}></button></li>
          <li><MediaControlButton action={this.handleToggleVideo}
                                  enabled={this.props.video.enabled}
                                  scope="local" type="video" /></li>
          <li><MediaControlButton action={this.handleToggleAudio}
                                  enabled={this.props.audio.enabled}
                                  scope="local" type="audio" /></li>
        </ul>
      );
    }
  });

  var ConversationView = React.createClass({
    mixins: [Backbone.Events],

    propTypes: {
      sdk: React.PropTypes.object.isRequired,
      model: React.PropTypes.object.isRequired
    },

    // height set to "auto" to fix video layout on Google Chrome
    // @see https://bugzilla.mozilla.org/show_bug.cgi?id=991122
    publisherConfig: {
      width: "100%",
      height: "auto",
      style: {
        bugDisplayMode: "off",
        buttonDisplayMode: "off",
        nameDisplayMode: "off"
      }
    },

    getInitialState: function() {
      return {
        video: {enabled: false},
        audio: {enabled: false}
      };
    },

    componentDidMount: function() {
      this.listenTo(this.props.model, "session:connected",
                                      this.startPublishing);
      this.listenTo(this.props.model, "session:stream-created",
                                      this._streamCreated);
      this.listenTo(this.props.model, ["session:peer-hungup",
                                       "session:network-disconnected",
                                       "session:ended"].join(" "),
                                       this.stopPublishing);

      this.props.model.startSession();
    },

    componentWillUnmount: function() {
      // Unregister all local event listeners
      this.stopListening();
      this.hangup();
    },

    hangup: function() {
      this.stopPublishing();
      this.props.model.endSession();
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
      var incoming = this.getDOMNode().querySelector(".incoming");
      event.streams.forEach(function(stream) {
        if (stream.connection.connectionId !==
            this.props.model.session.connection.connectionId) {
          this.props.model.session.subscribe(stream, incoming,
                                             this.publisherConfig);
        }
      }, this);
    },

    /**
     * Publishes remote streams available once a session is connected.
     *
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionConnectEvent.html
     *
     * @param  {SessionConnectEvent} event
     */
    startPublishing: function(event) {
      var outgoing = this.getDOMNode().querySelector(".outgoing");

      // XXX move this into its StreamingVideo component?
      this.publisher = this.props.sdk.initPublisher(
        outgoing, this.publisherConfig);

      // Suppress OT GuM custom dialog, see bug 1018875
      this.listenTo(this.publisher, "accessDialogOpened accessDenied",
                    function(event) {
                      event.preventDefault();
                    });

      this.listenTo(this.publisher, "streamCreated", function(event) {
        this.setState({
          audio: {enabled: event.stream.hasAudio},
          video: {enabled: event.stream.hasVideo}
        });
      }.bind(this));

      this.listenTo(this.publisher, "streamDestroyed", function() {
        this.setState({
          audio: {enabled: false},
          video: {enabled: false}
        });
      }.bind(this));

      this.props.model.session.publish(this.publisher);
    },

    /**
     * Toggles streaming status for a given stream type.
     *
     * @param  {String}  type     Stream type ("audio" or "video").
     * @param  {Boolean} enabled  Enabled stream flag.
     */
    publishStream: function(type, enabled) {
      if (type === "audio") {
        this.publisher.publishAudio(enabled);
        this.setState({audio: {enabled: enabled}});
      } else {
        this.publisher.publishVideo(enabled);
        this.setState({video: {enabled: enabled}});
      }
    },

    /**
     * Unpublishes local stream.
     */
    stopPublishing: function() {
      // Unregister listeners for publisher events
      this.stopListening(this.publisher);

      this.props.model.session.unpublish(this.publisher);
    },

    render: function() {
      return (
        <div className="conversation">
          <ConversationToolbar video={this.state.video}
                               audio={this.state.audio}
                               publishStream={this.publishStream}
                               hangup={this.hangup} />
          <div className="media nested">
            <div className="remote"><div className="incoming"></div></div>
            <div className="local"><div className="outgoing"></div></div>
          </div>
        </div>
      );
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
    ConversationToolbar: ConversationToolbar,
    MediaControlButton: MediaControlButton,
    NotificationListView: NotificationListView,
    NotificationView: NotificationView,
    UnsupportedBrowserView: UnsupportedBrowserView,
    UnsupportedDeviceView: UnsupportedDeviceView
  };
})(_, window.OT, document.webL10n || document.mozL10n);
