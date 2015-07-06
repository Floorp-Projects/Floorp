/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(_, mozL10n) {
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
      action: React.PropTypes.func.isRequired,
      enabled: React.PropTypes.bool.isRequired,
      scope: React.PropTypes.string.isRequired,
      title: React.PropTypes.string,
      type: React.PropTypes.string.isRequired,
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
      if (this.props.title) {
        return this.props.title;
      }

      var prefix = this.props.enabled ? "mute" : "unmute";
      var suffix = "button_title";
      var msgId = [prefix, this.props.scope, this.props.type, suffix].join("_");
      return mozL10n.get(msgId);
    },

    render: function() {
      return (
        <button className={this._getClasses()}
                onClick={this.handleClick}
                title={this._getTitle()}></button>
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
      state: React.PropTypes.string.isRequired,
      visible: React.PropTypes.bool.isRequired
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

      return mozL10n.get(prefix + "_screenshare_button_title");
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
          <ul className={dropdownMenuClasses} ref="menu">
            <li onClick={this._handleShareTabs}>
              {mozL10n.get("share_tabs_button_title2")}
            </li>
            <li className={windowSharingClasses} onClick={this._handleShareWindows}>
              {mozL10n.get("share_windows_button_title")}
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
        edit: {enabled: false, visible: false},
        screenShare: {state: SCREEN_SHARE_STATES.INACTIVE, visible: false},
        enableHangup: true
      };
    },

    propTypes: {
      audio: React.PropTypes.object.isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      edit: React.PropTypes.object.isRequired,
      enableHangup: React.PropTypes.bool,
      hangup: React.PropTypes.func.isRequired,
      hangupButtonLabel: React.PropTypes.string,
      onEditClick: React.PropTypes.func,
      publishStream: React.PropTypes.func.isRequired,
      screenShare: React.PropTypes.object,
      video: React.PropTypes.object.isRequired
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

    handleToggleEdit: function() {
      if (this.props.onEditClick) {
        this.props.onEditClick(!this.props.edit.enabled);
      }
    },

    _getHangupButtonLabel: function() {
      return this.props.hangupButtonLabel || mozL10n.get("hangup_button_caption2");
    },

    render: function() {
      return (
        <ul className="conversation-toolbar">
          <li className="conversation-toolbar-btn-box btn-hangup-entry">
            <button className="btn btn-hangup"
                    disabled={!this.props.enableHangup}
                    onClick={this.handleClickHangup}
                    title={mozL10n.get("hangup_button_title")}>
              {this._getHangupButtonLabel()}
            </button>
          </li>
          <li className="conversation-toolbar-btn-box">
            <MediaControlButton action={this.handleToggleVideo}
                                enabled={this.props.video.enabled}
                                scope="local" type="video"
                                visible={this.props.video.visible} />
          </li>
          <li className="conversation-toolbar-btn-box">
            <MediaControlButton action={this.handleToggleAudio}
                                enabled={this.props.audio.enabled}
                                scope="local" type="audio"
                                visible={this.props.audio.visible} />
          </li>
          <li className="conversation-toolbar-btn-box">
            <ScreenShareControlButton dispatcher={this.props.dispatcher}
                                      state={this.props.screenShare.state}
                                      visible={this.props.screenShare.visible} />
          </li>
          <li className="conversation-toolbar-btn-box btn-edit-entry">
            <MediaControlButton action={this.handleToggleEdit}
                                enabled={this.props.edit.enabled}
                                scope="local"
                                title={mozL10n.get(this.props.edit.enabled ?
                                  "context_edit_tooltip" : "context_hide_tooltip")}
                                type="edit"
                                visible={this.props.edit.visible} />
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
      audio: React.PropTypes.object,
      initiate: React.PropTypes.bool,
      isDesktop: React.PropTypes.bool,
      model: React.PropTypes.object.isRequired,
      sdk: React.PropTypes.object.isRequired,
      video: React.PropTypes.object
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

        this.listenTo(this.props.sdk, "exception", this._handleSdkException);

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
                    function(ev) {
                      ev.preventDefault();
                    });

      this.listenTo(this.publisher, "streamCreated", function(ev) {
        this.setState({
          audio: {enabled: ev.stream.hasAudio},
          video: {enabled: ev.stream.hasVideo}
        });
      });

      this.listenTo(this.publisher, "streamDestroyed", function() {
        this.setState({
          audio: {enabled: false},
          video: {enabled: false}
        });
      });

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
            <ConversationToolbar
              audio={this.state.audio}
              hangup={this.hangup}
              publishStream={this.publishStream}
              video={this.state.video} />
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
      key: React.PropTypes.number.isRequired,
      notification: React.PropTypes.object.isRequired
    },

    render: function() {
      var notification = this.props.notification;
      return (
        <div className="notificationContainer">
          <div className={"alert alert-" + notification.get("level")}
            key={this.props.key}>
            <span className="message">{notification.get("message")}</span>
          </div>
          <div className={"detailsBar details-" + notification.get("level")}
               hidden={!notification.get("details")}>
            <button className="detailsButton btn-info"
                    hidden={!notification.get("detailsButtonLabel") || !notification.get("detailsButtonCallback")}
                    onClick={notification.get("detailsButtonCallback")}>
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
      clearOnDocumentHidden: React.PropTypes.bool,
      notifications: React.PropTypes.object.isRequired
    },

    getDefaultProps: function() {
      return {clearOnDocumentHidden: false};
    },

    componentDidMount: function() {
      this.listenTo(this.props.notifications, "reset add remove", function() {
        this.forceUpdate();
      });
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
      additionalClass: React.PropTypes.string,
      caption: React.PropTypes.string.isRequired,
      children: React.PropTypes.element,
      disabled: React.PropTypes.bool,
      htmlId: React.PropTypes.string,
      onClick: React.PropTypes.func.isRequired
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
        <button className={cx(classObject)}
                disabled={this.props.disabled}
                id={this.props.htmlId}
                onClick={this.props.onClick}>
          <span className="button-caption">{this.props.caption}</span>
          {this.props.children}
        </button>
      );
    }
  });

  var ButtonGroup = React.createClass({
    propTypes: {
      additionalClass: React.PropTypes.string,
      children: React.PropTypes.oneOfType([
        React.PropTypes.element,
        React.PropTypes.arrayOf(React.PropTypes.element)
      ])
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
    propTypes: {
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

    componentWillReceiveProps: function(nextProps) {
      // Only change the state if the prop has changed, and if it is also
      // different from the state.
      if (this.props.checked !== nextProps.checked &&
          this.state.checked !== nextProps.checked) {
        this.setState({ checked: nextProps.checked });
      }
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
        wrapperClasses[this.props.additionalClass] = true;
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

  /**
   * Renders an avatar element for display when video is muted.
   */
  var AvatarView = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    render: function() {
        return <div className="avatar"/>;
    }
  });

  /**
   * Renders a loading spinner for when video content is not yet available.
   */
  var LoadingView = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    render: function() {
        return (
          <div className="loading-background">
            <div className="loading-stream"/>
          </div>
        );
    }
  });

  /**
   * Renders a url that's part of context on the display.
   *
   * @property {Boolean} allowClick         Set to true to allow the url to be clicked. If this
   *                                        is specified, then 'dispatcher' is also required.
   * @property {String}  description        The description for the context url.
   * @property {loop.Dispatcher} dispatcher
   * @property {Boolean} showContextTitle   Whether or not to show the "Let's talk about" title.
   * @property {String}  thumbnail          The thumbnail url (expected to be a data url) to
   *                                        display. If not specified, a fallback url will be
   *                                        shown.
   * @property {String}  url                The url to be displayed. If not present or invalid,
   *                                        then this view won't be displayed.
   * @property {Boolean} useDesktopPaths    Whether or not to use the desktop paths for for the
   *                                        fallback url.
   */
  var ContextUrlView = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      allowClick: React.PropTypes.bool.isRequired,
      description: React.PropTypes.string.isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher),
      showContextTitle: React.PropTypes.bool.isRequired,
      thumbnail: React.PropTypes.string,
      url: React.PropTypes.string,
      useDesktopPaths: React.PropTypes.bool.isRequired
    },

    /**
     * Dispatches an action to record when the link is clicked.
     */
    handleLinkClick: function() {
      if (!this.props.allowClick) {
        return;
      }

      this.props.dispatcher.dispatch(new sharedActions.RecordClick({
        linkInfo: "Shared URL"
      }));
    },

    /**
     * Renders the context title ("Let's talk about") if necessary.
     */
    renderContextTitle: function() {
      if (!this.props.showContextTitle) {
        return null;
      }

      return <p>{mozL10n.get("context_inroom_label")}</p>;
    },

    render: function() {
      var hostname;

      try {
        hostname = new URL(this.props.url).hostname;
      } catch (ex) {
        return null;
      }

      var thumbnail = this.props.thumbnail;

      if (!thumbnail) {
        thumbnail = this.props.useDesktopPaths ?
          "loop/shared/img/icons-16x16.svg#globe" :
          "shared/img/icons-16x16.svg#globe";
      }

      return (
        <div className="context-content">
          {this.renderContextTitle()}
          <div className="context-wrapper">
            <img className="context-preview" src={thumbnail} />
            <span className="context-description">
              {this.props.description}
              <a className="context-url"
                 href={this.props.allowClick ? this.props.url : null}
                 onClick={this.handleLinkClick}
                 rel="noreferrer"
                 target="_blank">{hostname}</a>
            </span>
          </div>
        </div>
      );
    }
  });

  /**
   * Renders a media element for display. This also handles displaying an avatar
   * instead of the video, and attaching a video stream to the video element.
   */
  var MediaView = React.createClass({
    // srcVideoObject should be ok for a shallow comparison, so we are safe
    // to use the pure render mixin here.
    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      displayAvatar: React.PropTypes.bool.isRequired,
      isLoading: React.PropTypes.bool.isRequired,
      mediaType: React.PropTypes.string.isRequired,
      posterUrl: React.PropTypes.string,
      // Expecting "local" or "remote".
      srcVideoObject: React.PropTypes.object
    },

    componentDidMount: function() {
      if (!this.props.displayAvatar) {
        this.attachVideo(this.props.srcVideoObject);
      }
    },

    componentDidUpdate: function() {
      if (!this.props.displayAvatar) {
        this.attachVideo(this.props.srcVideoObject);
      }
    },

    /**
     * Attaches a video stream from a donor video element to this component's
     * video element if the component is displaying one.
     *
     * @param {Object} srcVideoObject The src video object to clone the stream
     *                                from.
     *
     * XXX need to have a corresponding detachVideo or change this to syncVideo
     * to protect from leaks (bug 1171978)
     */
    attachVideo: function(srcVideoObject) {
      if (!srcVideoObject) {
        // Not got anything to display.
        return;
      }

      var videoElement = this.getDOMNode();

      if (videoElement.tagName.toLowerCase() !== "video") {
        // Must be displaying the avatar view, so don't try and attach video.
        return;
      }

      // Set the src of our video element
      var attrName = "";
      if ("srcObject" in videoElement) {
        // srcObject is according to the standard.
        attrName = "srcObject";
      } else if ("mozSrcObject" in videoElement) {
        // mozSrcObject is for Firefox
        attrName = "mozSrcObject";
      } else if ("src" in videoElement) {
        // src is for Chrome.
        attrName = "src";
      } else {
        console.error("Error attaching stream to element - no supported" +
                      "attribute found");
        return;
      }

      // If the object hasn't changed it, then don't reattach it.
      if (videoElement[attrName] !== srcVideoObject[attrName]) {
        videoElement[attrName] = srcVideoObject[attrName];
      }
      videoElement.play();
    },

    render: function() {
      if (this.props.isLoading) {
        return <LoadingView />;
      }

      if (this.props.displayAvatar) {
        return <AvatarView />;
      }

      if (!this.props.srcVideoObject && !this.props.posterUrl) {
        return <div className="no-video"/>;
      }

      var optionalPoster = {};
      if (this.props.posterUrl) {
        optionalPoster.poster = this.props.posterUrl;
      }

      // For now, always mute media. For local media, we should be muted anyway,
      // as we don't want to hear ourselves speaking.
      //
      // For remote media, we would ideally have this live video element in
      // control of the audio, but due to the current method of not rendering
      // the element at all when video is muted we have to rely on the hidden
      // dom element in the sdk driver to play the audio.
      // We might want to consider changing this if we add UI controls relating
      // to the remote audio at some stage in the future.
      return (
        <video {...optionalPoster}
               className={this.props.mediaType + "-video"}
               muted />
      );
    }
  });

  return {
    AvatarView: AvatarView,
    Button: Button,
    ButtonGroup: ButtonGroup,
    Checkbox: Checkbox,
    ContextUrlView: ContextUrlView,
    ConversationView: ConversationView,
    ConversationToolbar: ConversationToolbar,
    MediaControlButton: MediaControlButton,
    MediaView: MediaView,
    LoadingView: LoadingView,
    ScreenShareControlButton: ScreenShareControlButton,
    NotificationListView: NotificationListView
  };
})(_, navigator.mozL10n || document.mozL10n);
