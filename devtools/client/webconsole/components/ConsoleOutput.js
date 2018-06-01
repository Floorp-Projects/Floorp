/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const {initialize} = require("devtools/client/webconsole/actions/ui");

const {
  getAllMessagesById,
  getAllMessagesUiById,
  getAllMessagesTableDataById,
  getAllNetworkMessagesUpdateById,
  getVisibleMessages,
  getAllRepeatById,
} = require("devtools/client/webconsole/selectors/messages");
const MessageContainer = createFactory(require("devtools/client/webconsole/components/MessageContainer").MessageContainer);
const {
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/constants");
const {
  getInitialMessageCountForViewport
} = require("devtools/client/webconsole/utils/messages.js");

class ConsoleOutput extends Component {
  static get propTypes() {
    return {
      initialized: PropTypes.bool.isRequired,
      messages: PropTypes.object.isRequired,
      messagesUi: PropTypes.array.isRequired,
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
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onContextMenu = this.onContextMenu.bind(this);
  }

  componentDidMount() {
    scrollToBottom(this.outputNode);
    this.props.serviceContainer.attachRefToHud("outputScroller", this.outputNode);

    // Waiting for the next paint.
    new Promise(res => requestAnimationFrame(res))
      .then(() => {
        if (this.props.onFirstMeaningfulPaint) {
          this.props.onFirstMeaningfulPaint();
        }

        // Dispatching on next tick so we don't block on action execution.
        setTimeout(() => {
          this.props.dispatch(initialize());
        }, 0);
      });
  }

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
    // - we are reacting to the "initialize" action,
    //   and we are already scrolled to the bottom
    // - the number of messages displayed changed
    //   and we are already scrolled to the bottom
    // - the number of messages in the store changed
    //   and the new message is an evaluation result.
    const isNewMessageEvaluationResult = messagesDelta > 0 &&
      [...nextProps.messages.values()][nextProps.messages.size - 1].type
        === MESSAGE_TYPE.RESULT;

    this.shouldScrollBottom =
      (
        !this.props.initialized &&
        nextProps.initialized &&
        isScrolledToBottom(lastChild, outputNode)
      ) ||
      (isNewMessageEvaluationResult) ||
      (visibleMessagesDelta > 0 && isScrolledToBottom(lastChild, outputNode));
  }

  componentDidUpdate() {
    if (this.shouldScrollBottom) {
      scrollToBottom(this.outputNode);
    }
  }

  onContextMenu(e) {
    this.props.serviceContainer.openContextMenu(e);
    e.stopPropagation();
    e.preventDefault();
  }

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
      initialized,
    } = this.props;

    if (!initialized) {
      const numberMessagesFitViewport = getInitialMessageCountForViewport(window);
      if (numberMessagesFitViewport < visibleMessages.length) {
        visibleMessages = visibleMessages.slice(
          visibleMessages.length - numberMessagesFitViewport);
      }
    }

    const messageNodes = visibleMessages.map((messageId) => MessageContainer({
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
}

function scrollToBottom(node) {
  node.scrollTop = node.scrollHeight;
}

function isScrolledToBottom(outputNode, scrollNode) {
  const lastNodeHeight = outputNode.lastChild ?
                       outputNode.lastChild.clientHeight : 0;
  return scrollNode.scrollTop + scrollNode.clientHeight >=
         scrollNode.scrollHeight - lastNodeHeight / 2;
}

function mapStateToProps(state, props) {
  return {
    initialized: state.ui.initialized,
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
