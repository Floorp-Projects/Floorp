/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */
var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(_, l10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedModels = loop.shared.models;
  var sharedMixins = loop.shared.mixins;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

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
      enabled: React.PropTypes.bool.isRequired,
      visible: React.PropTypes.bool.isRequired
    },

    getDefaultProps: function() {
      return {enabled: true, visible: true};
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
        "transparent-button": true,
        "local-media": this.props.scope === "local",
        "muted": !this.props.enabled,
        "hide": !this.props.visible
      };
      classesObj["btn-mute-" + this.props.type] = true;
      return cx(classesObj);
    },

    _getTitle: function(enabled) {
      var prefix = this.props.enabled ? "mute" : "unmute";
      var suffix = "button_title";
      var msgId = [prefix, this.props.scope, this.props.type, suffix].join("_");
      return l10n.get(msgId);
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
   * Screen sharing control button.
   *
   * Required props:
   * - {loop.Dispatcher} dispatcher  The dispatcher instance
   * - {Boolean}         visible     Set to true to display the button
   * - {String}          state       One of the screen sharing states, see
   *                                 loop.shared.utils.SCREEN_SHARE_STATES
   */
  var ScreenShareControlButton = React.createClass({
    mixins: [sharedMixins.DropdownMenuMixin()],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      visible: React.PropTypes.bool.isRequired,
      state: React.PropTypes.string.isRequired
    },

    getInitialState: function() {
      var os = loop.shared.utils.getOS();
      var osVersion = loop.shared.utils.getOSVersion();
      // Disable screensharing on older OSX and Windows versions.
      if ((os.indexOf("mac") > -1 && osVersion.major <= 10 && osVersion.minor <= 6) ||
          (os.indexOf("win") > -1 && osVersion.major <= 5 && osVersion.minor <= 2)) {
        return { windowSharingDisabled: true };
      }
      return { windowSharingDisabled: false };
    },

    handleClick: function() {
      if (this.props.state === SCREEN_SHARE_STATES.ACTIVE) {
        this.props.dispatcher.dispatch(
          new sharedActions.EndScreenShare({}));
      } else {
        this.toggleDropdownMenu();
      }
    },

    _startScreenShare: function(type) {
      this.props.dispatcher.dispatch(new sharedActions.StartScreenShare({
        type: type
      }));
    },

    _handleShareTabs: function() {
      this._startScreenShare("browser");
    },

    _handleShareWindows: function() {
      this._startScreenShare("window");
    },

    _getTitle: function() {
      var prefix = this.props.state === SCREEN_SHARE_STATES.ACTIVE ?
        "active" : "inactive";

      return l10n.get(prefix + "_screenshare_button_title");
    },

    render: function() {
      if (!this.props.visible) {
        return null;
      }

      var cx = React.addons.classSet;

      var isActive = this.props.state === SCREEN_SHARE_STATES.ACTIVE;
      var screenShareClasses = cx({
        "btn": true,
        "btn-screen-share": true,
        "transparent-button": true,
        "menu-showing": this.state.showMenu,
        "active": isActive,
        "disabled": this.props.state === SCREEN_SHARE_STATES.PENDING
      });
      var dropdownMenuClasses = cx({
        "native-dropdown-menu": true,
        "conversation-window-dropdown": true,
        "hide": !this.state.showMenu,
        "visually-hidden": true
      });
      var windowSharingClasses = cx({
        "disabled": this.state.windowSharingDisabled
      });

      return (
        <div>
          <button className={screenShareClasses}
                  onClick={this.handleClick}
                  ref="menu-button"
                  title={this._getTitle()}>
            {isActive ? null : <span className="chevron"/>}
          </button>
          <ul ref="menu" className={dropdownMenuClasses}>
            <li onClick={this._handleShareTabs}>
              {l10n.get("share_tabs_button_title2")}
            </li>
            <li onClick={this._handleShareWindows} className={windowSharingClasses}>
              {l10n.get("share_windows_button_title")}
            </li>
          </ul>
        </div>
      );
    }
  });

  /**
   * Conversation controls.
   */
  var ConversationToolbar = React.createClass({
    getDefaultProps: function() {
      return {
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true},
        screenShare: {state: SCREEN_SHARE_STATES.INACTIVE, visible: false},
        enableHangup: true
      };
    },

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      video: React.PropTypes.object.isRequired,
      audio: React.PropTypes.object.isRequired,
      screenShare: React.PropTypes.object,
      hangup: React.PropTypes.func.isRequired,
      publishStream: React.PropTypes.func.isRequired,
      hangupButtonLabel: React.PropTypes.string,
      enableHangup: React.PropTypes.bool
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

    _getHangupButtonLabel: function() {
      return this.props.hangupButtonLabel || l10n.get("hangup_button_caption2");
    },

    render: function() {
      return (
        <ul className="conversation-toolbar">
          <li className="conversation-toolbar-btn-box btn-hangup-entry">
            <button className="btn btn-hangup" onClick={this.handleClickHangup}
                    title={l10n.get("hangup_button_title")}
                    disabled={!this.props.enableHangup}>
              {this._getHangupButtonLabel()}
            </button>
          </li>
          <li className="conversation-toolbar-btn-box">
            <MediaControlButton action={this.handleToggleVideo}
                                enabled={this.props.video.enabled}
                                visible={this.props.video.visible}
                                scope="local" type="video" />
          </li>
          <li className="conversation-toolbar-btn-box">
            <MediaControlButton action={this.handleToggleAudio}
                                enabled={this.props.audio.enabled}
                                visible={this.props.audio.visible}
                                scope="local" type="audio" />
          </li>
          <li className="conversation-toolbar-btn-box btn-screen-share-entry">
            <ScreenShareControlButton dispatcher={this.props.dispatcher}
                                      visible={this.props.screenShare.visible}
                                      state={this.props.screenShare.state} />
          </li>
        </ul>
      );
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = React.createClass({
    mixins: [
      Backbone.Events,
      sharedMixins.AudioMixin,
      sharedMixins.MediaSetupMixin
    ],

    propTypes: {
      sdk: React.PropTypes.object.isRequired,
      video: React.PropTypes.object,
      audio: React.PropTypes.object,
      initiate: React.PropTypes.bool,
      isDesktop: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {
        initiate: true,
        isDesktop: false,
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true}
      };
    },

    getInitialState: function() {
      return {
        video: this.props.video,
        audio: this.props.audio
      };
    },

    componentDidMount: function() {
      if (this.props.initiate) {
        /**
         * XXX This is a workaround for desktop machines that do not have a
         * camera installed. As we don't yet have device enumeration, when
         * we do, this can be removed (bug 1138851), and the sdk should handle it.
         */
        if (this.props.isDesktop &&
            !window.MediaStreamTrack.getSources) {
          // If there's no getSources function, the sdk defines its own and caches
          // the result. So here we define the "normal" one which doesn't get cached, so
          // we can change it later.
          window.MediaStreamTrack.getSources = function(callback) {
            callback([{kind: "audio"}, {kind: "video"}]);
          };
        }

        this.listenTo(this.props.sdk, "exception", this._handleSdkException.bind(this));

        this.listenTo(this.props.model, "session:connected",
                                        this._onSessionConnected);
        this.listenTo(this.props.model, "session:stream-created",
                                        this._streamCreated);
        this.listenTo(this.props.model, ["session:peer-hungup",
                                         "session:network-disconnected",
                                         "session:ended"].join(" "),
                                         this.stopPublishing);
        this.props.model.startSession();
      }
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

    _onSessionConnected: function(event) {
      this.startPublishing(event);
      this.play("connected");
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
      var incoming = this.getDOMNode().querySelector(".remote");
      this.props.model.subscribe(event.stream, incoming,
        this.getDefaultPublisherConfig({
          publishVideo: this.props.video.enabled
        }));
    },

    /**
     * Handles the SDK Exception event.
     *
     * https://tokbox.com/opentok/libraries/client/js/reference/ExceptionEvent.html
     *
     * @param {ExceptionEvent} event
     */
    _handleSdkException: function(event) {
      /**
       * XXX This is a workaround for desktop machines that do not have a
       * camera installed. As we don't yet have device enumeration, when
       * we do, this can be removed (bug 1138851), and the sdk should handle it.
       */
      if (this.publisher &&
          event.code === OT.ExceptionCodes.UNABLE_TO_PUBLISH &&
          event.message === "GetUserMedia" &&
          this.state.video.enabled) {
        this.state.video.enabled = false;

        window.MediaStreamTrack.getSources = function(callback) {
          callback([{kind: "audio"}]);
        };

        this.stopListening(this.publisher);
        this.publisher.destroy();
        this.startPublishing();
      }
    },

    /**
     * Publishes remote streams available once a session is connected.
     *
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionConnectEvent.html
     *
     * @param  {SessionConnectEvent} event
     */
    startPublishing: function(event) {
      var outgoing = this.getDOMNode().querySelector(".local");

      // XXX move this into its StreamingVideo component?
      this.publisher = this.props.sdk.initPublisher(
        outgoing, this.getDefaultPublisherConfig({publishVideo: this.props.video.enabled}));

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

      this.props.model.publish(this.publisher);
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
      if (this.publisher) {
        // Unregister listeners for publisher events
        this.stopListening(this.publisher);

        this.props.model.session.unpublish(this.publisher);
      }
    },

    render: function() {
      var localStreamClasses = React.addons.classSet({
        local: true,
        "local-stream": true,
        "local-stream-audio": !this.state.video.enabled
      });
      return (
        <div className="video-layout-wrapper">
          <div className="conversation in-call">
            <div className="media nested">
              <div className="video_wrapper remote_wrapper">
                <div className="video_inner remote focus-stream"></div>
              </div>
              <div className={localStreamClasses}></div>
            </div>
            <ConversationToolbar video={this.state.video}
                                 audio={this.state.audio}
                                 publishStream={this.publishStream}
                                 hangup={this.hangup} />
          </div>
        </div>
      );
    }
  });

  /**
   * Notification view.
   */
  var NotificationView = React.createClass({
    mixins: [Backbone.Events],

    propTypes: {
      notification: React.PropTypes.object.isRequired,
      key: React.PropTypes.number.isRequired
    },

    render: function() {
      var notification = this.props.notification;
      return (
        <div className="notificationContainer">
          <div key={this.props.key}
               className={"alert alert-" + notification.get("level")}>
            <span className="message">{notification.get("message")}</span>
          </div>
          <div className={"detailsBar details-" + notification.get("level")}
               hidden={!notification.get("details")}>
            <button className="detailsButton btn-info"
                    onClick={notification.get("detailsButtonCallback")}
                    hidden={!notification.get("detailsButtonLabel") || !notification.get("detailsButtonCallback")}>
              {notification.get("detailsButtonLabel")}
            </button>
            <span className="details">{notification.get("details")}</span>
          </div>
        </div>
      );
    }
  });

  /**
   * Notification list view.
   */
  var NotificationListView = React.createClass({
    mixins: [Backbone.Events, sharedMixins.DocumentVisibilityMixin],

    propTypes: {
      notifications: React.PropTypes.object.isRequired,
      clearOnDocumentHidden: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {clearOnDocumentHidden: false};
    },

    componentDidMount: function() {
      this.listenTo(this.props.notifications, "reset add remove", function() {
        this.forceUpdate();
      }.bind(this));
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.notifications);
    },

    /**
     * Provided by DocumentVisibilityMixin. Clears notifications stack when the
     * current document is hidden if the clearOnDocumentHidden prop is set to
     * true and the collection isn't empty.
     */
    onDocumentHidden: function() {
      if (this.props.clearOnDocumentHidden &&
          this.props.notifications.length > 0) {
        // Note: The `silent` option prevents the `reset` event to be triggered
        // here, preventing the UI to "jump" a little because of the event
        // callback being processed in another tick (I think).
        this.props.notifications.reset([], {silent: true});
        this.forceUpdate();
      }
    },

    render: function() {
      return (
        <div className="messages">{
          this.props.notifications.map(function(notification, key) {
            return <NotificationView key={key} notification={notification}/>;
          })
        }
        </div>
      );
    }
  });

  var Button = React.createClass({
    propTypes: {
      caption: React.PropTypes.string.isRequired,
      onClick: React.PropTypes.func.isRequired,
      disabled: React.PropTypes.bool,
      additionalClass: React.PropTypes.string,
      htmlId: React.PropTypes.string
    },

    getDefaultProps: function() {
      return {
        disabled: false,
        additionalClass: "",
        htmlId: ""
      };
    },

    render: function() {
      var cx = React.addons.classSet;
      var classObject = { button: true, disabled: this.props.disabled };
      if (this.props.additionalClass) {
        classObject[this.props.additionalClass] = true;
      }
      return (
        <button onClick={this.props.onClick}
                disabled={this.props.disabled}
                id={this.props.htmlId}
                className={cx(classObject)}>
          <span className="button-caption">{this.props.caption}</span>
          {this.props.children}
        </button>
      );
    }
  });

  var ButtonGroup = React.createClass({
    PropTypes: {
      additionalClass: React.PropTypes.string
    },

    getDefaultProps: function() {
      return {
        additionalClass: ""
      };
    },

    render: function() {
      var cx = React.addons.classSet;
      var classObject = { "button-group": true };
      if (this.props.additionalClass) {
        classObject[this.props.additionalClass] = true;
      }
      return (
        <div className={cx(classObject)}>
          {this.props.children}
        </div>
      );
    }
  });

  var Checkbox = React.createClass({
    PropTypes: {
      additionalClass: React.PropTypes.string,
      checked: React.PropTypes.bool,
      disabled: React.PropTypes.bool,
      label: React.PropTypes.string,
      onChange: React.PropTypes.func.isRequired,
      // If `value` is not supplied, the consumer should rely on the boolean
      // `checked` state changes.
      value: React.PropTypes.string
    },

    getDefaultProps: function() {
      return {
        additionalClass: "",
        checked: false,
        disabled: false,
        label: null,
        value: ""
      };
    },

    getInitialState: function() {
      return {
        checked: this.props.checked,
        value: this.props.checked ? this.props.value : ""
      };
    },

    _handleClick: function(event) {
      event.preventDefault();

      var newState = {
        checked: !this.state.checked,
        value: this.state.checked ? "" : this.props.value
      };
      this.setState(newState);
      this.props.onChange(newState);
    },

    render: function() {
      var cx = React.addons.classSet;
      var wrapperClasses = {
        "checkbox-wrapper": true,
        disabled: this.props.disabled
      };
      var checkClasses = {
        checkbox: true,
        checked: this.state.checked,
        disabled: this.props.disabled
      };
      if (this.props.additionalClass) {
        checkClasses[this.props.additionalClass] = true;
      }
      return (
        <div className={cx(wrapperClasses)}
             disabled={this.props.disabled}
             onClick={this._handleClick}>
          <div className={cx(checkClasses)} />
          {this.props.label ?
            <label>{this.props.label}</label> :
            null}
        </div>
      );
    }
  });

  return {
    Button: Button,
    ButtonGroup: ButtonGroup,
    Checkbox: Checkbox,
    ConversationView: ConversationView,
    ConversationToolbar: ConversationToolbar,
    MediaControlButton: MediaControlButton,
    ScreenShareControlButton: ScreenShareControlButton,
    NotificationListView: NotificationListView
  };
})(_, navigator.mozL10n || document.mozL10n);
