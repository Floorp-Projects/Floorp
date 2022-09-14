/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { PluralForm } = require("devtools/shared/plural-form");
const {
  getDisplayedMessages,
  isCurrentChannelClosed,
  getClosedConnectionDetails,
} = require("devtools/client/netmonitor/src/selectors/index");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { table, tbody, tr, td, div, input, label, hr, p } = dom;
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const MESSAGES_EMPTY_TEXT = L10N.getStr("messagesEmptyText");
const TOGGLE_MESSAGES_TRUNCATION = L10N.getStr("toggleMessagesTruncation");
const TOGGLE_MESSAGES_TRUNCATION_TITLE = L10N.getStr(
  "toggleMessagesTruncation.title"
);
const CONNECTION_CLOSED_TEXT = L10N.getStr("netmonitor.ws.connection.closed");
const {
  MESSAGE_HEADERS,
} = require("devtools/client/netmonitor/src/constants.js");
const Actions = require("devtools/client/netmonitor/src/actions/index");

const {
  getSelectedMessage,
} = require("devtools/client/netmonitor/src/selectors/index");

// Components
const MessageListContextMenu = require("devtools/client/netmonitor/src/components/messages/MessageListContextMenu");
loader.lazyGetter(this, "MessageListHeader", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/MessageListHeader")
  );
});
loader.lazyGetter(this, "MessageListItem", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/MessageListItem")
  );
});

const LEFT_MOUSE_BUTTON = 0;

/**
 * Renders the actual contents of the message list.
 */
class MessageListContent extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      startPanelContainer: PropTypes.object,
      messages: PropTypes.array,
      selectedMessage: PropTypes.object,
      selectMessage: PropTypes.func.isRequired,
      columns: PropTypes.object.isRequired,
      isClosed: PropTypes.bool.isRequired,
      closedConnectionDetails: PropTypes.object,
      channelId: PropTypes.number,
      onSelectMessageDelta: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onContextMenu = this.onContextMenu.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.messagesLimit = Services.prefs.getIntPref(
      "devtools.netmonitor.msg.displayed-messages.limit"
    );
    this.currentTruncatedNum = 0;
    this.state = {
      checked: false,
    };
    this.pinnedToBottom = false;
    this.initIntersectionObserver = false;
    this.intersectionObserver = null;
    this.toggleTruncationCheckBox = this.toggleTruncationCheckBox.bind(this);
  }

  componentDidMount() {
    const { startPanelContainer } = this.props;
    const { scrollAnchor } = this.refs;

    if (scrollAnchor) {
      // Always scroll to anchor when MessageListContent component first mounts.
      scrollAnchor.scrollIntoView();
    }
    this.setupScrollToBottom(startPanelContainer, scrollAnchor);
  }

  componentDidUpdate(prevProps) {
    const { startPanelContainer, channelId } = this.props;
    const { scrollAnchor } = this.refs;

    // When messages are cleared, the previous scrollAnchor would be destroyed, so we need to reset this boolean.
    if (!scrollAnchor) {
      this.initIntersectionObserver = false;
    }

    // In addition to that, we need to reset currentTruncatedNum
    if (prevProps.messages.length && this.props.messages.length === 0) {
      this.currentTruncatedNum = 0;
    }

    // If a new connection is selected, scroll to anchor.
    if (channelId !== prevProps.channelId && scrollAnchor) {
      scrollAnchor.scrollIntoView();
    }

    // Do not autoscroll if the selection changed. This would cause
    // the newly selected message to jump just after clicking in.
    // (not user friendly)
    //
    // If the selection changed, we need to ensure that the newly
    // selected message is properly scrolled into the visible area.
    if (prevProps.selectedMessage === this.props.selectedMessage) {
      this.setupScrollToBottom(startPanelContainer, scrollAnchor);
    } else {
      const head = document.querySelector("thead.message-list-headers-group");
      const selectedRow = document.querySelector(
        "tr.message-list-item.selected"
      );

      if (selectedRow) {
        const rowRect = selectedRow.getBoundingClientRect();
        const scrollableRect = startPanelContainer.getBoundingClientRect();
        const headRect = head.getBoundingClientRect();

        if (rowRect.top <= scrollableRect.top) {
          selectedRow.scrollIntoView(true);

          // We need to scroll a bit more to get the row out
          // of the header. The header is sticky and overlaps
          // part of the scrollable area.
          startPanelContainer.scrollTop -= headRect.height;
        } else if (rowRect.bottom > scrollableRect.bottom) {
          selectedRow.scrollIntoView(false);
        }
      }
    }
  }

  componentWillUnmount() {
    // Reset observables and boolean values.
    const { scrollAnchor } = this.refs;

    if (this.intersectionObserver) {
      if (scrollAnchor) {
        this.intersectionObserver.unobserve(scrollAnchor);
      }
      this.initIntersectionObserver = false;
      this.pinnedToBottom = false;
    }
  }

  setupScrollToBottom(startPanelContainer, scrollAnchor) {
    if (startPanelContainer && scrollAnchor) {
      // Initialize intersection observer.
      if (!this.initIntersectionObserver) {
        this.intersectionObserver = new IntersectionObserver(
          () => {
            // When scrollAnchor first comes into view, this.pinnedToBottom is set to true.
            // When the anchor goes out of view, this callback function triggers again and toggles this.pinnedToBottom.
            // Subsequent scroll into/out of view will toggle this.pinnedToBottom.
            this.pinnedToBottom = !this.pinnedToBottom;
          },
          {
            root: startPanelContainer,
            threshold: 0.1,
          }
        );
        if (this.intersectionObserver) {
          this.intersectionObserver.observe(scrollAnchor);
          this.initIntersectionObserver = true;
        }
      }

      if (this.pinnedToBottom) {
        scrollAnchor.scrollIntoView();
      }
    }
  }

  toggleTruncationCheckBox() {
    this.setState({
      checked: !this.state.checked,
    });
  }

  onMouseDown(evt, item) {
    if (evt.button === LEFT_MOUSE_BUTTON) {
      this.props.selectMessage(item);
    }
  }

  onContextMenu(evt, item) {
    evt.preventDefault();
    const { connector } = this.props;
    this.contextMenu = new MessageListContextMenu({
      connector,
    });
    this.contextMenu.open(evt, item);
  }

  /**
   * Handler for keyboard events. For arrow up/down, page up/down, home/end,
   * move the selection up or down.
   */
  onKeyDown(evt) {
    evt.preventDefault();
    evt.stopPropagation();
    let delta;

    switch (evt.key) {
      case "ArrowUp":
        delta = -1;
        break;
      case "ArrowDown":
        delta = +1;
        break;
      case "PageUp":
        delta = "PAGE_UP";
        break;
      case "PageDown":
        delta = "PAGE_DOWN";
        break;
      case "Home":
        delta = -Infinity;
        break;
      case "End":
        delta = +Infinity;
        break;
    }

    if (delta) {
      this.props.onSelectMessageDelta(delta);
    }
  }

  render() {
    const {
      messages,
      selectedMessage,
      connector,
      columns,
      isClosed,
      closedConnectionDetails,
    } = this.props;

    if (messages.length === 0) {
      return div(
        { className: "empty-notice message-list-empty-notice" },
        MESSAGES_EMPTY_TEXT
      );
    }

    const visibleColumns = MESSAGE_HEADERS.filter(
      header => columns[header.name]
    ).map(col => col.name);

    let displayedMessages;
    let MESSAGES_TRUNCATED;
    const shouldTruncate = messages.length > this.messagesLimit;
    if (shouldTruncate) {
      // If the checkbox is checked, we display all messages after the currentTruncatedNum limit.
      // If the checkbox is unchecked, we display all messages after the messagesLimit.
      this.currentTruncatedNum = this.state.checked
        ? this.currentTruncatedNum
        : messages.length - this.messagesLimit;
      displayedMessages = messages.slice(this.currentTruncatedNum);

      MESSAGES_TRUNCATED = PluralForm.get(
        this.currentTruncatedNum,
        L10N.getStr("netmonitor.ws.truncated-messages.warning")
      ).replace("#1", this.currentTruncatedNum);
    } else {
      displayedMessages = messages;
    }

    let connectionClosedMsg = CONNECTION_CLOSED_TEXT;
    if (
      closedConnectionDetails &&
      closedConnectionDetails.code !== undefined &&
      closedConnectionDetails.reason !== undefined
    ) {
      connectionClosedMsg += `: ${closedConnectionDetails.code} ${closedConnectionDetails.reason}`;
    }
    return div(
      {},
      table(
        { className: "message-list-table" },
        MessageListHeader(),
        tbody(
          {
            className: "message-list-body",
            onKeyDown: this.onKeyDown,
          },
          tr(
            {
              tabIndex: 0,
            },
            td(
              {
                className: "truncated-messages-cell",
                colSpan: visibleColumns.length,
              },
              shouldTruncate &&
                div(
                  {
                    className: "truncated-messages-header",
                  },
                  div(
                    {
                      className: "truncated-messages-container",
                    },
                    div({
                      className: "truncated-messages-warning-icon",
                    }),
                    div(
                      {
                        className: "truncated-message",
                        title: MESSAGES_TRUNCATED,
                      },
                      MESSAGES_TRUNCATED
                    )
                  ),
                  label(
                    {
                      className: "truncated-messages-checkbox-label",
                      title: TOGGLE_MESSAGES_TRUNCATION_TITLE,
                    },
                    input({
                      type: "checkbox",
                      className: "truncation-checkbox",
                      title: TOGGLE_MESSAGES_TRUNCATION_TITLE,
                      checked: this.state.checked,
                      onChange: this.toggleTruncationCheckBox,
                    }),
                    TOGGLE_MESSAGES_TRUNCATION
                  )
                )
            )
          ),
          displayedMessages.map((item, index) =>
            MessageListItem({
              key: "message-list-item-" + index,
              item,
              index,
              isSelected: item === selectedMessage,
              onMouseDown: evt => this.onMouseDown(evt, item),
              onContextMenu: evt => this.onContextMenu(evt, item),
              connector,
              visibleColumns,
            })
          )
        )
      ),
      isClosed &&
        p(
          {
            className: "msg-connection-closed-message",
          },
          connectionClosedMsg
        ),
      hr({
        ref: "scrollAnchor",
        className: "message-list-scroll-anchor",
      })
    );
  }
}

module.exports = connect(
  state => ({
    selectedMessage: getSelectedMessage(state),
    messages: getDisplayedMessages(state),
    columns: state.messages.columns,
    isClosed: isCurrentChannelClosed(state),
    closedConnectionDetails: getClosedConnectionDetails(state),
  }),
  dispatch => ({
    selectMessage: item => dispatch(Actions.selectMessage(item)),
    onSelectMessageDelta: delta => dispatch(Actions.selectMessageDelta(delta)),
  })
)(MessageListContent);
