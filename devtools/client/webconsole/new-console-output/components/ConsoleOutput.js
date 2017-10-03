/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createClass,
  createFactory,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  getAllMessagesById,
  getAllMessagesUiById,
  getAllMessagesTableDataById,
  getAllNetworkMessagesUpdateById,
  getVisibleMessages,
  getAllRepeatById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const MessageContainer = createFactory(require("devtools/client/webconsole/new-console-output/components/MessageContainer").MessageContainer);
const {
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/new-console-output/constants");

const ConsoleOutput = createClass({

  displayName: "ConsoleOutput",

  propTypes: {
    messages: PropTypes.object.isRequired,
    messagesUi: PropTypes.object.isRequired,
    serviceContainer: PropTypes.shape({
      attachRefToHud: PropTypes.func.isRequired,
      openContextMenu: PropTypes.func.isRequired,
      sourceMapService: PropTypes.object,
    }),
    dispatch: PropTypes.func.isRequired,
    timestampsVisible: PropTypes.bool,
    messagesTableData: PropTypes.object.isRequired,
    messagesRepeat: PropTypes.object.isRequired,
    networkMessagesUpdate: PropTypes.object.isRequired,
    visibleMessages: PropTypes.array.isRequired,
    networkMessageActiveTabId: PropTypes.string.isRequired,
  },

  componentDidMount() {
    // Do the scrolling in the nextTick since this could hit console startup performances.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1355869
    setTimeout(() => {
      scrollToBottom(this.outputNode);
    }, 0);
    this.props.serviceContainer.attachRefToHud("outputScroller", this.outputNode);
  },

  componentWillUpdate(nextProps, nextState) {
    const outputNode = this.outputNode;
    if (!outputNode || !outputNode.lastChild) {
      // Force a scroll to bottom when messages are added to an empty console.
      // This makes the console stay pinned to the bottom if a batch of messages
      // are added after a page refresh (Bug 1402237).
      this.shouldScrollBottom = true;
      return;
    }

    const lastChild = outputNode.lastChild;
    const visibleMessagesDelta =
      nextProps.visibleMessages.length - this.props.visibleMessages.length;
    const messagesDelta =
      nextProps.messages.size - this.props.messages.size;

    // We need to scroll to the bottom if:
    // - the number of messages displayed changed
    //   and we are already scrolled to the bottom
    // - the number of messages in the store changed
    //   and the new message is an evaluation result.
    this.shouldScrollBottom =
      (messagesDelta > 0 && nextProps.messages.last().type === MESSAGE_TYPE.RESULT) ||
      (visibleMessagesDelta > 0 && isScrolledToBottom(lastChild, outputNode));
  },

  componentDidUpdate() {
    if (this.shouldScrollBottom) {
      scrollToBottom(this.outputNode);
    }
  },

  onContextMenu(e) {
    this.props.serviceContainer.openContextMenu(e);
    e.stopPropagation();
    e.preventDefault();
  },

  render() {
    let {
      dispatch,
      visibleMessages,
      messages,
      messagesUi,
      messagesTableData,
      messagesRepeat,
      networkMessagesUpdate,
      networkMessageActiveTabId,
      serviceContainer,
      timestampsVisible,
    } = this.props;

    let messageNodes = visibleMessages.map((messageId) => MessageContainer({
      dispatch,
      key: messageId,
      messageId,
      serviceContainer,
      open: messagesUi.includes(messageId),
      tableData: messagesTableData.get(messageId),
      timestampsVisible,
      repeat: messagesRepeat[messageId],
      networkMessageUpdate: networkMessagesUpdate[messageId],
      networkMessageActiveTabId,
      getMessage: () => messages.get(messageId),
    }));

    return (
      dom.div({
        className: "webconsole-output",
        onContextMenu: this.onContextMenu,
        ref: node => {
          this.outputNode = node;
        },
      }, messageNodes
      )
    );
  }
});

function scrollToBottom(node) {
  node.scrollTop = node.scrollHeight;
}

function isScrolledToBottom(outputNode, scrollNode) {
  let lastNodeHeight = outputNode.lastChild ?
                       outputNode.lastChild.clientHeight : 0;
  return scrollNode.scrollTop + scrollNode.clientHeight >=
         scrollNode.scrollHeight - lastNodeHeight / 2;
}

function mapStateToProps(state, props) {
  return {
    messages: getAllMessagesById(state),
    visibleMessages: getVisibleMessages(state),
    messagesUi: getAllMessagesUiById(state),
    messagesTableData: getAllMessagesTableDataById(state),
    messagesRepeat: getAllRepeatById(state),
    networkMessagesUpdate: getAllNetworkMessagesUpdateById(state),
    timestampsVisible: state.ui.timestampsVisible,
    networkMessageActiveTabId: state.ui.networkMessageActiveTabId,
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
