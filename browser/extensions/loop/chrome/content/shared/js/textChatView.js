var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = loop.shared.views || {};
loop.shared.views.chat = function (mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedViews = loop.shared.views;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;

  /**
   * Renders an individual entry for the text chat entries view.
   */
  var TextChatEntry = React.createClass({
    displayName: "TextChatEntry",

    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      contentType: React.PropTypes.string.isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher),
      extraData: React.PropTypes.object,
      message: React.PropTypes.string.isRequired,
      showTimestamp: React.PropTypes.bool.isRequired,
      timestamp: React.PropTypes.string.isRequired,
      type: React.PropTypes.string.isRequired
    },

    /**
     * Pretty print timestamp. From time in milliseconds to HH:MM
     * (or L10N equivalent).
     *
     */
    _renderTimestamp: function () {
      var date = new Date(this.props.timestamp);

      return React.createElement(
        "span",
        { className: "text-chat-entry-timestamp" },
        date.toLocaleTimeString(mozL10n.language.code, { hour: "numeric", minute: "numeric",
          hour12: false })
      );
    },

    render: function () {
      var classes = classNames({
        "text-chat-entry": this.props.contentType !== CHAT_CONTENT_TYPES.NOTIFICATION,
        "received": this.props.type === CHAT_MESSAGE_TYPES.RECEIVED,
        "sent": this.props.type === CHAT_MESSAGE_TYPES.SENT,
        "special": this.props.type === CHAT_MESSAGE_TYPES.SPECIAL,
        "room-name": this.props.contentType === CHAT_CONTENT_TYPES.ROOM_NAME,
        "text-chat-notif": this.props.contentType === CHAT_CONTENT_TYPES.NOTIFICATION
      });

      var optionalProps = {};
      if (loop.shared.utils.isDesktop()) {
        optionalProps.linkClickHandler = function (url) {
          loop.request("OpenURL", url);
        };
      }

      if (this.props.contentType === CHAT_CONTENT_TYPES.CONTEXT_TILE) {
        return React.createElement(
          "div",
          { className: classes },
          React.createElement(sharedViews.ContextUrlView, {
            allowClick: true,
            description: this.props.message,
            dispatcher: this.props.dispatcher,
            thumbnail: this.props.extraData.newRoomThumbnail,
            url: this.props.extraData.newRoomURL }),
          this.props.showTimestamp ? this._renderTimestamp() : null
        );
      }

      if (this.props.contentType === CHAT_CONTENT_TYPES.NOTIFICATION) {
        return React.createElement(
          "div",
          { className: classes },
          React.createElement(
            "div",
            { className: "content-wrapper" },
            React.createElement("img", { className: "notification-icon", src: "shared/img/leave_notification.svg" }),
            React.createElement(
              "p",
              null,
              mozL10n.get(this.props.message)
            )
          ),
          this.props.showTimestamp ? this._renderTimestamp() : null
        );
      }

      return React.createElement(
        "div",
        { className: classes },
        React.createElement(sharedViews.LinkifiedTextView, _extends({}, optionalProps, {
          rawText: this.props.message })),
        React.createElement("span", { className: "text-chat-arrow" }),
        this.props.showTimestamp ? this._renderTimestamp() : null
      );
    }
  });

  var TextChatRoomName = React.createClass({
    displayName: "TextChatRoomName",

    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      message: React.PropTypes.string.isRequired
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "text-chat-header special room-name" },
        React.createElement(
          "p",
          null,
          mozL10n.get("rooms_welcome_title", { conversationName: this.props.message })
        )
      );
    }
  });

  /**
   * Manages the text entries in the chat entries view. This is split out from
   * TextChatView so that scrolling can be managed more efficiently - this
   * component only updates when the message list is changed.
   */
  var TextChatEntriesView = React.createClass({
    displayName: "TextChatEntriesView",

    mixins: [React.addons.PureRenderMixin, sharedMixins.AudioMixin],

    statics: {
      ONE_MINUTE: 60
    },

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      messageList: React.PropTypes.arrayOf(React.PropTypes.object).isRequired,
      showInitialContext: React.PropTypes.bool.isRequired
    },

    getInitialState: function () {
      return {
        receivedMessageCount: 0
      };
    },

    _hasChatMessages: function () {
      return this.props.messageList.some(function (message) {
        return message.contentType !== CHAT_CONTENT_TYPES.ROOM_NAME && message.contentType !== CHAT_CONTENT_TYPES.CONTEXT;
      });
    },

    componentWillUpdate: function () {
      var node = this.getDOMNode();
      if (!node) {
        return;
      }
      // Scroll only if we're right at the bottom of the display, or if we've
      // not had any chat messages so far.
      this.shouldScroll = !this._hasChatMessages() || node.scrollHeight === node.scrollTop + node.clientHeight;
    },

    componentWillReceiveProps: function (nextProps) {
      var receivedMessageCount = nextProps.messageList.filter(function (message) {
        return message.type === CHAT_MESSAGE_TYPES.RECEIVED;
      }).length;

      // If the number of received messages has increased, we play a sound.
      if (receivedMessageCount > this.state.receivedMessageCount) {
        this.play("message");
        this.setState({ receivedMessageCount: receivedMessageCount });
      }
    },

    componentDidUpdate: function () {
      // Don't scroll if we haven't got any chat messages yet - e.g. for context
      // display, we want to display starting at the top.
      if (this.shouldScroll && this._hasChatMessages()) {
        // This ensures the paint is complete.
        window.requestAnimationFrame(function () {
          try {
            var node = this.getDOMNode();
            node.scrollTop = node.scrollHeight - node.clientHeight;
          } catch (ex) {
            console.error("TextChatEntriesView.componentDidUpdate exception", ex);
          }
        }.bind(this));
      }
    },

    render: function () {
      /* Keep track of the last printed timestamp. */
      var lastTimestamp = 0;

      var entriesClasses = classNames({
        "text-chat-entries": true
      });

      return React.createElement(
        "div",
        { className: entriesClasses },
        React.createElement(
          "div",
          { className: "text-chat-scroller" },
          this.props.messageList.map(function (entry, i) {
            if (entry.type === CHAT_MESSAGE_TYPES.SPECIAL) {
              if (!this.props.showInitialContext) {
                return null;
              }
              switch (entry.contentType) {
                case CHAT_CONTENT_TYPES.ROOM_NAME:
                  return React.createElement(TextChatRoomName, {
                    key: i,
                    message: entry.message });
                case CHAT_CONTENT_TYPES.CONTEXT:
                  return React.createElement(
                    "div",
                    { className: "context-url-view-wrapper", key: i },
                    React.createElement(sharedViews.ContextUrlView, {
                      allowClick: true,
                      description: entry.message,
                      dispatcher: this.props.dispatcher,
                      thumbnail: entry.extraData.thumbnail,
                      url: entry.extraData.location })
                  );
                default:
                  console.error("Unsupported contentType", entry.contentType);
                  return null;
              }
            }

            /* For SENT messages there is no received timestamp. */
            var timestamp = entry.receivedTimestamp || entry.sentTimestamp;

            var timeDiff = this._isOneMinDelta(timestamp, lastTimestamp);
            var shouldShowTimestamp = this._shouldShowTimestamp(i, timeDiff);

            if (shouldShowTimestamp) {
              lastTimestamp = timestamp;
            }

            return React.createElement(TextChatEntry, { contentType: entry.contentType,
              dispatcher: this.props.dispatcher,
              extraData: entry.extraData,
              key: i,
              message: entry.message,
              showTimestamp: shouldShowTimestamp,
              timestamp: timestamp,
              type: entry.type });
          }, this)
        )
      );
    },

    /**
     * Decide to show timestamp or not on a message.
     * If the time difference between two consecutive messages is bigger than
     * one minute or if message types are different.
     *
     * @param {number} idx       Index of message in the messageList.
     * @param {boolean} timeDiff If difference between consecutive messages is
     *                           bigger than one minute.
     */
    _shouldShowTimestamp: function (idx, timeDiff) {
      if (!idx) {
        return true;
      }

      /* If consecutive messages are from different senders */
      if (this.props.messageList[idx].type !== this.props.messageList[idx - 1].type) {
        return true;
      }

      return timeDiff;
    },

    /**
     * Determines if difference between the two timestamp arguments
     * is bigger that 60 (1 minute)
     *
     * Timestamps are using ISO8601 format.
     *
     * @param {string} currTime Timestamp of message yet to be rendered.
     * @param {string} prevTime Last timestamp printed in the chat view.
     */
    _isOneMinDelta: function (currTime, prevTime) {
      var date1 = new Date(currTime);
      var date2 = new Date(prevTime);
      var delta = date1 - date2;

      if (delta / 1000 >= this.constructor.ONE_MINUTE) {
        return true;
      }

      return false;
    }
  });

  /**
   * Displays a text chat entry input box for sending messages.
   *
   * @property {loop.Dispatcher} dispatcher
   * @property {Boolean} showPlaceholder    Set to true to show the placeholder message.
   * @property {Boolean} textChatEnabled    Set to true to enable the box. If false, the
   *                                        text chat box won't be displayed.
   */
  var TextChatInputView = React.createClass({
    displayName: "TextChatInputView",

    mixins: [React.addons.LinkedStateMixin, React.addons.PureRenderMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      showPlaceholder: React.PropTypes.bool.isRequired,
      textChatEnabled: React.PropTypes.bool.isRequired
    },

    getInitialState: function () {
      return {
        messageDetail: ""
      };
    },

    /**
     * Handles a key being pressed - looking for the return key for submitting
     * the form.
     *
     * @param {Object} event The DOM event.
     */
    handleKeyDown: function (event) {
      if (event.which === 13) {
        this.handleFormSubmit(event);
      }
    },

    /**
     * Handles submitting of the form - dispatches a send text chat message.
     *
     * @param {Object} event The DOM event.
     */
    handleFormSubmit: function (event) {
      event.preventDefault();

      // Don't send empty messages.
      if (!this.state.messageDetail) {
        return;
      }

      this.props.dispatcher.dispatch(new sharedActions.SendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: this.state.messageDetail,
        sentTimestamp: new Date().toISOString()
      }));

      // Reset the form to empty, ready for the next message.
      this.setState({ messageDetail: "" });
    },

    render: function () {
      if (!this.props.textChatEnabled) {
        return null;
      }

      return React.createElement(
        "div",
        { className: "text-chat-box" },
        React.createElement(
          "form",
          { onSubmit: this.handleFormSubmit },
          React.createElement("input", {
            onKeyDown: this.handleKeyDown,
            placeholder: this.props.showPlaceholder ? mozL10n.get("chat_textbox_placeholder") : "",
            type: "text",
            valueLink: this.linkState("messageDetail") })
        )
      );
    }
  });

  /**
   * Displays the text chat view. This includes the text chat messages as well
   * as a field for entering new messages.
   *
   * @property {loop.Dispatcher} dispatcher
   * @property {Boolean}         showInitialContext Set to true to show the room name
   *                                          and initial context tile for linker clicker's special list items
   */
  var TextChatView = React.createClass({
    displayName: "TextChatView",

    mixins: [React.addons.LinkedStateMixin, loop.store.StoreMixin("textChatStore")],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      showInitialContext: React.PropTypes.bool.isRequired,
      showTile: React.PropTypes.bool.isRequired
    },

    getInitialState: function () {
      return this.getStoreState();
    },

    render: function () {
      var messageList = this.state.messageList;

      // Filter out items not displayed when showing initial context.
      // We do this here so that we can set the classes correctly on the view.
      if (!this.props.showInitialContext) {
        messageList = messageList.filter(function (item) {
          return item.type !== CHAT_MESSAGE_TYPES.SPECIAL || item.contentType !== CHAT_CONTENT_TYPES.ROOM_NAME && item.contentType !== CHAT_CONTENT_TYPES.CONTEXT;
        });
      }

      // Only show the placeholder if we've sent messages.
      var hasSentMessages = messageList.some(function (item) {
        return item.type === CHAT_MESSAGE_TYPES.SENT;
      });

      var textChatViewClasses = classNames({
        "text-chat-view": true,
        "text-chat-entries-empty": !messageList.length,
        "text-chat-disabled": !this.state.textChatEnabled
      });

      return React.createElement(
        "div",
        { className: textChatViewClasses },
        React.createElement(TextChatEntriesView, {
          dispatcher: this.props.dispatcher,
          messageList: messageList,
          showInitialContext: this.props.showInitialContext }),
        React.createElement(TextChatInputView, {
          dispatcher: this.props.dispatcher,
          showPlaceholder: !hasSentMessages,
          textChatEnabled: this.state.textChatEnabled }),
        React.createElement(sharedViews.AdsTileView, {
          dispatcher: this.props.dispatcher,
          showTile: this.props.showTile })
      );
    }
  });

  return {
    TextChatEntriesView: TextChatEntriesView,
    TextChatEntry: TextChatEntry,
    TextChatView: TextChatView
  };
}(navigator.mozL10n || document.mozL10n);
