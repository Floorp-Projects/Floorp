/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = loop.shared.views || {};
loop.shared.views.TextChatView = (function(mozL10n) {
  var sharedActions = loop.shared.actions;
  var sharedViews = loop.shared.views;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES;

  /**
   * Renders an individual entry for the text chat entries view.
   */
  var TextChatEntry = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      contentType: React.PropTypes.string.isRequired,
      message: React.PropTypes.string.isRequired,
      type: React.PropTypes.string.isRequired
    },

    render: function() {
      var classes = React.addons.classSet({
        "text-chat-entry": true,
        "received": this.props.type === CHAT_MESSAGE_TYPES.RECEIVED,
        "special": this.props.type === CHAT_MESSAGE_TYPES.SPECIAL,
        "room-name": this.props.contentType === CHAT_CONTENT_TYPES.ROOM_NAME
      });

      return (
        <div className={classes}>
          <p>{this.props.message}</p>
        </div>
      );
    }
  });

  var TextChatRoomName = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      message: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        <div className="text-chat-entry special room-name">
          <p>{mozL10n.get("rooms_welcome_title", {conversationName: this.props.message})}</p>
        </div>
      );
    }
  });

  /**
   * Manages the text entries in the chat entries view. This is split out from
   * TextChatView so that scrolling can be managed more efficiently - this
   * component only updates when the message list is changed.
   */
  var TextChatEntriesView = React.createClass({
    mixins: [React.addons.PureRenderMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      messageList: React.PropTypes.array.isRequired
    },

    componentWillUpdate: function() {
      var node = this.getDOMNode();
      if (!node) {
        return;
      }
      // Scroll only if we're right at the bottom of the display.
      this.shouldScroll = node.scrollHeight === node.scrollTop + node.clientHeight;
    },

    componentDidUpdate: function() {
      if (this.shouldScroll) {
        // This ensures the paint is complete.
        window.requestAnimationFrame(function() {
          var node = this.getDOMNode();
          node.scrollTop = node.scrollHeight - node.clientHeight;
        }.bind(this));
      }
    },

    render: function() {
      if (!this.props.messageList.length) {
        return null;
      }

      return (
        <div className="text-chat-entries">
          <div className="text-chat-scroller">
            {
              this.props.messageList.map(function(entry, i) {
                if (entry.type === CHAT_MESSAGE_TYPES.SPECIAL) {
                  switch (entry.contentType) {
                    case CHAT_CONTENT_TYPES.ROOM_NAME:
                      return <TextChatRoomName message={entry.message}/>;
                    case CHAT_CONTENT_TYPES.CONTEXT:
                      return (
                        <div className="context-url-view-wrapper">
                          <sharedViews.ContextUrlView
                            allowClick={true}
                            description={entry.message}
                            dispatcher={this.props.dispatcher}
                            key={i}
                            showContextTitle={true}
                            thumbnail={entry.extraData.thumbnail}
                            url={entry.extraData.location}
                            useDesktopPaths={false} />
                        </div>
                      );
                    default:
                      console.error("Unsupported contentType", entry.contentType);
                      return null;
                  }
                }

                return (
                  <TextChatEntry key={i}
                                 contentType={entry.contentType}
                                 message={entry.message}
                                 type={entry.type} />
                );
              }, this)
            }
          </div>
        </div>
      );
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
    mixins: [
      React.addons.LinkedStateMixin,
      React.addons.PureRenderMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      showPlaceholder: React.PropTypes.bool.isRequired,
      textChatEnabled: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
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
    handleKeyDown: function(event) {
      if (event.which === 13) {
        this.handleFormSubmit(event);
      }
    },

    /**
     * Handles submitting of the form - dispatches a send text chat message.
     *
     * @param {Object} event The DOM event.
     */
    handleFormSubmit: function(event) {
      event.preventDefault();

      // Don't send empty messages.
      if (!this.state.messageDetail) {
        return;
      }

      this.props.dispatcher.dispatch(new sharedActions.SendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: this.state.messageDetail
      }));

      // Reset the form to empty, ready for the next message.
      this.setState({ messageDetail: "" });
    },

    render: function() {
      if (!this.props.textChatEnabled) {
        return null;
      }

      return (
        <div className="text-chat-box">
          <form onSubmit={this.handleFormSubmit}>
            <input type="text"
                   placeholder={this.props.showPlaceholder ? mozL10n.get("chat_textbox_placeholder") : ""}
                   onKeyDown={this.handleKeyDown}
                   valueLink={this.linkState("messageDetail")} />
          </form>
        </div>
      );
    }
  });

  /**
   * Displays the text chat view. This includes the text chat messages as well
   * as a field for entering new messages.
   *
   * @property {loop.Dispatcher} dispatcher
   * @property {Boolean} showAlways         If false, the view will not be rendered
   *                                        if text chat is not enabled and the
   *                                        message list is empty.
   * @property {Boolean} showRoomName       Set to true to show the room name special
   *                                        list item.
   */
  var TextChatView = React.createClass({
    mixins: [
      React.addons.LinkedStateMixin,
      loop.store.StoreMixin("textChatStore")
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      showAlways: React.PropTypes.bool.isRequired,
      showRoomName: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      return this.getStoreState();
    },

    render: function() {
      var messageList;
      var hasNonSpecialMessages;

      if (this.props.showRoomName) {
        messageList = this.state.messageList;
        hasNonSpecialMessages = messageList.some(function(item) {
          return item.type !== CHAT_MESSAGE_TYPES.SPECIAL;
        });
      } else {
        // XXX Desktop should be showing the initial context here (bug 1171940).
        messageList = this.state.messageList.filter(function(item) {
          return item.type !== CHAT_MESSAGE_TYPES.SPECIAL;
        });
        hasNonSpecialMessages = !!messageList.length;
      }

      if (!this.props.showAlways && !this.state.textChatEnabled && !messageList.length) {
        return null;
      }

      return (
        <div className="text-chat-view">
          <TextChatEntriesView
            dispatcher={this.props.dispatcher}
            messageList={messageList} />
          <TextChatInputView
            dispatcher={this.props.dispatcher}
            showPlaceholder={!hasNonSpecialMessages}
            textChatEnabled={this.state.textChatEnabled} />
        </div>
      );
    }
  });

  return TextChatView;
})(navigator.mozL10n || document.mozL10n);
