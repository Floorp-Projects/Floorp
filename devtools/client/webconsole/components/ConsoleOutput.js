/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createElement } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const {initialize} = require("devtools/client/webconsole/actions/ui");

const {
  getAllMessagesById,
  getAllMessagesUiById,
  getAllMessagesPayloadById,
  getAllMessagesTableDataById,
  getAllNetworkMessagesUpdateById,
  getVisibleMessages,
  getPausedExecutionPoint,
  getAllRepeatById,
  getAllWarningGroupsById,
  isMessageInWarningGroup,
} = require("devtools/client/webconsole/selectors/messages");

loader.lazyRequireGetter(this, "PropTypes", "devtools/client/shared/vendor/react-prop-types");
loader.lazyRequireGetter(this, "MessageContainer", "devtools/client/webconsole/components/MessageContainer", true);
ChromeUtils.defineModuleGetter(this, "pointPrecedes", "resource://devtools/shared/execution-point-utils.js");

const {
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/constants");
const {
  getInitialMessageCountForViewport,
} = require("devtools/client/webconsole/utils/messages.js");

function getClosestMessage(visibleMessages, messages, executionPoint) {
  if (!executionPoint || !visibleMessages) {
    return null;
  }

  const messageList = visibleMessages.map(id => messages.get(id));
  const precedingMessages = messageList.filter(m => {
    return m && m.executionPoint && pointPrecedes(m.executionPoint, executionPoint);
  });
  if (precedingMessages.length != 0) {
    return precedingMessages.sort((a, b) => {
      return pointPrecedes(a.executionPoint, b.executionPoint);
    })[0];
  }
  return messageList.filter(m => m && m.executionPoint).sort((a, b) => {
    return pointPrecedes(b.executionPoint, a.executionPoint);
  })[0];
}

class ConsoleOutput extends Component {
  static get propTypes() {
    return {
      initialized: PropTypes.bool.isRequired,
      messages: PropTypes.object.isRequired,
      messagesUi: PropTypes.array.isRequired,
      serviceContainer: PropTypes.shape({
        attachRefToWebConsoleUI: PropTypes.func.isRequired,
        openContextMenu: PropTypes.func.isRequired,
        sourceMapService: PropTypes.object,
      }),
      dispatch: PropTypes.func.isRequired,
      timestampsVisible: PropTypes.bool,
      messagesPayload: PropTypes.object.isRequired,
      messagesTableData: PropTypes.object.isRequired,
      messagesRepeat: PropTypes.object.isRequired,
      warningGroups: PropTypes.object.isRequired,
      isInWarningGroup: PropTypes.func,
      networkMessagesUpdate: PropTypes.object.isRequired,
      visibleMessages: PropTypes.array.isRequired,
      networkMessageActiveTabId: PropTypes.string.isRequired,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      pausedExecutionPoint: PropTypes.any,
    };
  }

  constructor(props) {
    super(props);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.maybeScrollToBottom = this.maybeScrollToBottom.bind(this);
  }

  componentDidMount() {
    if (this.props.visibleMessages.length > 0) {
      scrollToBottom(this.outputNode);
    }

    const {
      serviceContainer,
      onFirstMeaningfulPaint,
      dispatch,
    } = this.props;
    serviceContainer.attachRefToWebConsoleUI("outputScroller", this.outputNode);

    // Waiting for the next paint.
    new Promise(res => requestAnimationFrame(res))
      .then(() => {
        if (onFirstMeaningfulPaint) {
          onFirstMeaningfulPaint();
        }

        // Dispatching on next tick so we don't block on action execution.
        setTimeout(() => {
          dispatch(initialize());
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

    // We need to scroll to the bottom if:
    // - we are reacting to "initialize" action, and we are already scrolled to the bottom
    // - the number of messages displayed changed and we are already scrolled to the
    //   bottom, but not if we are reacting to a group opening.
    // - the number of messages in the store changed and the new message is an evaluation
    //   result.

    const lastChild = outputNode.lastChild;
    const visibleMessagesDelta =
      nextProps.visibleMessages.length - this.props.visibleMessages.length;
    const messagesDelta =
      nextProps.messages.size - this.props.messages.size;
    const isNewMessageEvaluationResult = messagesDelta > 0 &&
      [...nextProps.messages.values()][nextProps.messages.size - 1].type
        === MESSAGE_TYPE.RESULT;

    const messagesUiDelta =
      nextProps.messagesUi.length - this.props.messagesUi.length;
    const isOpeningGroup = messagesUiDelta > 0 &&
      nextProps.messagesUi.some(id =>
        !this.props.messagesUi.includes(id) &&
        nextProps.messagesUi.includes(id) &&
        this.props.visibleMessages.includes(id) &&
        nextProps.visibleMessages.includes(id));

    this.shouldScrollBottom =
      (
        !this.props.initialized &&
        nextProps.initialized &&
        isScrolledToBottom(lastChild, outputNode)
      ) ||
      (isNewMessageEvaluationResult) ||
      (
        isScrolledToBottom(lastChild, outputNode) &&
        visibleMessagesDelta > 0 &&
        !isOpeningGroup
      );
  }

  componentDidUpdate() {
    this.maybeScrollToBottom();
  }

  maybeScrollToBottom() {
    if (this.outputNode && this.shouldScrollBottom) {
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
      messagesPayload,
      messagesTableData,
      messagesRepeat,
      warningGroups,
      isInWarningGroup,
      networkMessagesUpdate,
      networkMessageActiveTabId,
      serviceContainer,
      timestampsVisible,
      initialized,
      pausedExecutionPoint,
    } = this.props;

    if (!initialized) {
      const numberMessagesFitViewport = getInitialMessageCountForViewport(window);
      if (numberMessagesFitViewport < visibleMessages.length) {
        visibleMessages = visibleMessages.slice(
          visibleMessages.length - numberMessagesFitViewport);
      }
    }

    const pausedMessage = getClosestMessage(
      visibleMessages, messages, pausedExecutionPoint);

    const messageNodes = visibleMessages.map((messageId) =>
      createElement(MessageContainer, {
        dispatch,
        key: messageId,
        messageId,
        serviceContainer,
        open: messagesUi.includes(messageId),
        payload: messagesPayload.get(messageId),
        tableData: messagesTableData.get(messageId),
        timestampsVisible,
        repeat: messagesRepeat[messageId],
        badge: warningGroups.has(messageId) ? warningGroups.get(messageId).length : null,
        inWarningGroup: isInWarningGroup
          ? isInWarningGroup(messages.get(messageId))
          : false,
        networkMessageUpdate: networkMessagesUpdate[messageId],
        networkMessageActiveTabId,
        pausedExecutionPoint,
        getMessage: () => messages.get(messageId),
        isPaused: !!pausedMessage && pausedMessage.id == messageId,
        maybeScrollToBottom: this.maybeScrollToBottom,
      }));

    return (
      dom.div({
        className: "webconsole-output",
        role: "main",
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
  if (node.scrollHeight > node.clientHeight) {
    node.scrollTop = node.scrollHeight;
  }
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
    pausedExecutionPoint: getPausedExecutionPoint(state),
    messages: getAllMessagesById(state),
    visibleMessages: getVisibleMessages(state),
    messagesUi: getAllMessagesUiById(state),
    messagesPayload: getAllMessagesPayloadById(state),
    messagesTableData: getAllMessagesTableDataById(state),
    messagesRepeat: getAllRepeatById(state),
    warningGroups: getAllWarningGroupsById(state),
    isInWarningGroup: state.prefs.groupWarnings
      ? message => isMessageInWarningGroup(state, message)
      : null,
    networkMessagesUpdate: getAllNetworkMessagesUpdateById(state),
    timestampsVisible: state.ui.timestampsVisible,
    networkMessageActiveTabId: state.ui.networkMessageActiveTabId,
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
