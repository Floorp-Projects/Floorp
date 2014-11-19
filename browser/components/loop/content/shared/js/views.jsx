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
  var sharedMixins = loop.shared.mixins;

  var WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS = 5;

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
        /* jshint ignore:start */
        <button className={this._getClasses()}
                title={this._getTitle()}
                onClick={this.handleClick}></button>
        /* jshint ignore:end */
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
        enableHangup: true
      };
    },

    propTypes: {
      video: React.PropTypes.object.isRequired,
      audio: React.PropTypes.object.isRequired,
      hangup: React.PropTypes.func.isRequired,
      publishStream: React.PropTypes.func.isRequired,
      hangupButtonLabel: React.PropTypes.string,
      enableHangup: React.PropTypes.bool,
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
      var cx = React.addons.classSet;
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
        </ul>
      );
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = React.createClass({
    mixins: [Backbone.Events, sharedMixins.AudioMixin],

    propTypes: {
      sdk: React.PropTypes.object.isRequired,
      video: React.PropTypes.object,
      audio: React.PropTypes.object,
      initiate: React.PropTypes.bool
    },

    // height set to 100%" to fix video layout on Google Chrome
    // @see https://bugzilla.mozilla.org/show_bug.cgi?id=1020445
    publisherConfig: {
      insertMode: "append",
      width: "100%",
      height: "100%",
      style: {
        audioLevelDisplayMode: "off",
        bugDisplayMode: "off",
        buttonDisplayMode: "off",
        nameDisplayMode: "off",
        videoDisabledDisplayMode: "off"
      }
    },

    getDefaultProps: function() {
      return {
        initiate: true,
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

    componentWillMount: function() {
      if (this.props.initiate) {
        this.publisherConfig.publishVideo = this.props.video.enabled;
      }
    },

    componentDidMount: function() {
      if (this.props.initiate) {
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

      /**
       * OT inserts inline styles into the markup. Using a listener for
       * resize events helps us trigger a full width/height on the element
       * so that they update to the correct dimensions.
       * XXX: this should be factored as a mixin.
       */
      window.addEventListener('orientationchange', this.updateVideoContainer);
      window.addEventListener('resize', this.updateVideoContainer);
    },

    updateVideoContainer: function() {
      var localStreamParent = document.querySelector('.local .OT_publisher');
      var remoteStreamParent = document.querySelector('.remote .OT_subscriber');
      if (localStreamParent) {
        localStreamParent.style.width = "100%";
      }
      if (remoteStreamParent) {
        remoteStreamParent.style.height = "100%";
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
      this.props.model.subscribe(event.stream, incoming, this.publisherConfig);
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
      /* jshint ignore:start */
      return (
        <div className="video-layout-wrapper">
          <div className="conversation">
            <div className="media nested">
              <div className="video_wrapper remote_wrapper">
                <div className="video_inner remote"></div>
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
      /* jshint ignore:end */
    }
  });

  /**
   * Feedback outer layout.
   *
   * Props:
   * -
   */
  var FeedbackLayout = React.createClass({
    propTypes: {
      children: React.PropTypes.component.isRequired,
      title: React.PropTypes.string.isRequired,
      reset: React.PropTypes.func // if not specified, no Back btn is shown
    },

    render: function() {
      var backButton = <div />;
      if (this.props.reset) {
        backButton = (
          <button className="fx-embedded-btn-back" type="button"
                  onClick={this.props.reset}>
            &laquo;&nbsp;{l10n.get("feedback_back_button")}
          </button>
        );
      }
      return (
        <div className="feedback">
          {backButton}
          <h3>{this.props.title}</h3>
          {this.props.children}
        </div>
      );
    }
  });

  /**
   * Detailed feedback form.
   */
  var FeedbackForm = React.createClass({
    propTypes: {
      pending:      React.PropTypes.bool,
      sendFeedback: React.PropTypes.func,
      reset:        React.PropTypes.func
    },

    getInitialState: function() {
      return {category: "", description: ""};
    },

    getDefaultProps: function() {
      return {pending: false};
    },

    _getCategories: function() {
      return {
        audio_quality: l10n.get("feedback_category_audio_quality"),
        video_quality: l10n.get("feedback_category_video_quality"),
        disconnected : l10n.get("feedback_category_was_disconnected"),
        confusing:     l10n.get("feedback_category_confusing"),
        other:         l10n.get("feedback_category_other")
      };
    },

    _getCategoryFields: function() {
      var categories = this._getCategories();
      return Object.keys(categories).map(function(category, key) {
        return (
          <label key={key} className="feedback-category-label">
            <input type="radio" ref="category" name="category"
                   className="feedback-category-radio"
                   value={category}
                   onChange={this.handleCategoryChange}
                   checked={this.state.category === category} />
            {categories[category]}
          </label>
        );
      }, this);
    },

    /**
     * Checks if the form is ready for submission:
     *
     * - no feedback submission should be pending.
     * - a category (reason) must be chosen;
     * - if the "other" category is chosen, a custom description must have been
     *   entered by the end user;
     *
     * @return {Boolean}
     */
    _isFormReady: function() {
      if (this.props.pending || !this.state.category) {
        return false;
      }
      if (this.state.category === "other" && !this.state.description) {
        return false;
      }
      return true;
    },

    handleCategoryChange: function(event) {
      var category = event.target.value;
      this.setState({
        category: category,
        description: category == "other" ? "" : this._getCategories()[category]
      });
      if (category == "other") {
        this.refs.description.getDOMNode().focus();
      }
    },

    handleDescriptionFieldChange: function(event) {
      this.setState({description: event.target.value});
    },

    handleDescriptionFieldFocus: function(event) {
      this.setState({category: "other", description: ""});
    },

    handleFormSubmit: function(event) {
      event.preventDefault();
      this.props.sendFeedback({
        happy: false,
        category: this.state.category,
        description: this.state.description
      });
    },

    render: function() {
      var descriptionDisplayValue = this.state.category === "other" ?
                                    this.state.description : "";
      return (
        <FeedbackLayout title={l10n.get("feedback_what_makes_you_sad")}
                        reset={this.props.reset}>
          <form onSubmit={this.handleFormSubmit}>
            {this._getCategoryFields()}
            <p>
              <input type="text" ref="description" name="description"
                className="feedback-description"
                onChange={this.handleDescriptionFieldChange}
                onFocus={this.handleDescriptionFieldFocus}
                value={descriptionDisplayValue}
                placeholder={
                  l10n.get("feedback_custom_category_text_placeholder")} />
            </p>
            <button type="submit" className="btn btn-success"
                    disabled={!this._isFormReady()}>
              {l10n.get("feedback_submit_button")}
            </button>
          </form>
        </FeedbackLayout>
      );
    }
  });

  /**
   * Feedback received view.
   *
   * Props:
   * - {Function} onAfterFeedbackReceived Function to execute after the
   *   WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS timeout has elapsed
   */
  var FeedbackReceived = React.createClass({
    propTypes: {
      onAfterFeedbackReceived: React.PropTypes.func
    },

    getInitialState: function() {
      return {countdown: WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS};
    },

    componentDidMount: function() {
      this._timer = setInterval(function() {
        this.setState({countdown: this.state.countdown - 1});
      }.bind(this), 1000);
    },

    componentWillUnmount: function() {
      if (this._timer) {
        clearInterval(this._timer);
      }
    },

    render: function() {
      if (this.state.countdown < 1) {
        clearInterval(this._timer);
        if (this.props.onAfterFeedbackReceived) {
          this.props.onAfterFeedbackReceived();
        }
      }
      return (
        <FeedbackLayout title={l10n.get("feedback_thank_you_heading")}>
          <p className="info thank-you">{
            l10n.get("feedback_window_will_close_in2", {
              countdown: this.state.countdown,
              num: this.state.countdown
            })}</p>
        </FeedbackLayout>
      );
    }
  });

  /**
   * Feedback view.
   */
  var FeedbackView = React.createClass({
    mixins: [sharedMixins.AudioMixin],

    propTypes: {
      // A loop.FeedbackAPIClient instance
      feedbackApiClient: React.PropTypes.object.isRequired,
      onAfterFeedbackReceived: React.PropTypes.func,
      // The current feedback submission flow step name
      step: React.PropTypes.oneOf(["start", "form", "finished"])
    },

    getInitialState: function() {
      return {pending: false, step: this.props.step || "start"};
    },

    getDefaultProps: function() {
      return {step: "start"};
    },

    componentDidMount: function() {
      this.play("terminated");
    },

    reset: function() {
      this.setState(this.getInitialState());
    },

    handleHappyClick: function() {
      this.sendFeedback({happy: true}, this._onFeedbackSent);
    },

    handleSadClick: function() {
      this.setState({step: "form"});
    },

    sendFeedback: function(fields) {
      // Setting state.pending to true will disable the submit button to avoid
      // multiple submissions
      this.setState({pending: true});
      // Sends feedback data
      this.props.feedbackApiClient.send(fields, this._onFeedbackSent);
    },

    _onFeedbackSent: function(err) {
      if (err) {
        // XXX better end user error reporting, see bug 1046738
        console.error("Unable to send user feedback", err);
      }
      this.setState({pending: false, step: "finished"});
    },

    render: function() {
      switch(this.state.step) {
        case "finished":
          return (
            <FeedbackReceived
              onAfterFeedbackReceived={this.props.onAfterFeedbackReceived} />
          );
        case "form":
          return <FeedbackForm feedbackApiClient={this.props.feedbackApiClient}
                               sendFeedback={this.sendFeedback}
                               reset={this.reset}
                               pending={this.state.pending} />;
        default:
          return (
            <FeedbackLayout title={
              l10n.get("feedback_call_experience_heading2")}>
              <div className="faces">
                <button className="face face-happy"
                        onClick={this.handleHappyClick}></button>
                <button className="face face-sad"
                        onClick={this.handleSadClick}></button>
              </div>
            </FeedbackLayout>
          );
      }
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
      htmlId: React.PropTypes.string,
    },

    getDefaultProps: function() {
      return {
        disabled: false,
        additionalClass: "",
        htmlId: "",
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
      )
    }
  });

  var ButtonGroup = React.createClass({
    PropTypes: {
      additionalClass: React.PropTypes.string
    },

    getDefaultProps: function() {
      return {
        additionalClass: "",
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
      )
    }
  });

  return {
    Button: Button,
    ButtonGroup: ButtonGroup,
    ConversationView: ConversationView,
    ConversationToolbar: ConversationToolbar,
    FeedbackView: FeedbackView,
    MediaControlButton: MediaControlButton,
    NotificationListView: NotificationListView
  };
})(_, window.OT, navigator.mozL10n || document.mozL10n);
