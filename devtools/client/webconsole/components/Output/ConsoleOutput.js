/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createElement,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { initialize } = require("devtools/client/webconsole/actions/ui");

const {
  getAllMessagesById,
  getAllMessagesUiById,
  getAllMessagesPayloadById,
  getAllNetworkMessagesUpdateById,
  getVisibleMessages,
  getAllRepeatById,
  getAllWarningGroupsById,
  isMessageInWarningGroup,
} = require("devtools/client/webconsole/selectors/messages");

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "MessageContainer",
  "devtools/client/webconsole/components/Output/MessageContainer",
  true
);

const { MESSAGE_TYPE } = require("devtools/client/webconsole/constants");
const {
  getInitialMessageCountForViewport,
} = require("devtools/client/webconsole/utils/messages.js");

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
      messagesRepeat: PropTypes.object.isRequired,
      warningGroups: PropTypes.object.isRequired,
      networkMessagesUpdate: PropTypes.object.isRequired,
      visibleMessages: PropTypes.array.isRequired,
      networkMessageActiveTabId: PropTypes.string.isRequired,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      editorMode: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.maybeScrollToBottom = this.maybeScrollToBottom.bind(this);

    this.resizeObserver = new ResizeObserver(entries => {
      // If we don't have the outputNode reference, or if the outputNode isn't connected
      // anymore, we disconnect the resize observer (componentWillUnmount is never called
      // on this component, so we have to do it here).
      if (!this.outputNode || !this.outputNode.isConnected) {
        this.resizeObserver.disconnect();
        return;
      }

      if (this.scrolledToBottom) {
        this.scrollToBottom();
      }
    });
  }

  componentDidMount() {
    if (this.props.visibleMessages.length > 0) {
      this.scrollToBottom();
    }

    this.lastMessageIntersectionObserver = new IntersectionObserver(
      entries => {
        for (const entry of entries) {
          // Consider that we're not pinned to the bottom anymore if the last message is
          // less than half-visible.
          this.scrolledToBottom = entry.intersectionRatio >= 0.5;
        }
      },
      { root: this.outputNode, threshold: [0.5] }
    );

    this.resizeObserver.observe(this.getElementToObserve());

    const { serviceContainer, onFirstMeaningfulPaint, dispatch } = this.props;
    serviceContainer.attachRefToWebConsoleUI("outputScroller", this.outputNode);

    // Waiting for the next paint.
    new Promise(res => requestAnimationFrame(res)).then(() => {
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
    if (nextProps.editorMode !== this.props.editorMode) {
      this.resizeObserver.disconnect();
    }

    const { outputNode } = this;
    if (!outputNode?.lastChild) {
      // Force a scroll to bottom when messages are added to an empty console.
      // This makes the console stay pinned to the bottom if a batch of messages
      // are added after a page refresh (Bug 1402237).
      this.shouldScrollBottom = true;
      return;
    }

    const { lastChild } = outputNode;
    this.lastMessageIntersectionObserver.unobserve(lastChild);

    // We need to scroll to the bottom if:
    // - we are reacting to "initialize" action, and we are already scrolled to the bottom
    // - the number of messages displayed changed and we are already scrolled to the
    //   bottom, but not if we are reacting to a group opening.
    // - the number of messages in the store changed and the new message is an evaluation
    //   result.

    const visibleMessagesDelta =
      nextProps.visibleMessages.length - this.props.visibleMessages.length;
    const messagesDelta = nextProps.messages.size - this.props.messages.size;
    const isNewMessageEvaluationResult =
      messagesDelta > 0 &&
      [...nextProps.messages.values()][nextProps.messages.size - 1].type ===
        MESSAGE_TYPE.RESULT;

    const messagesUiDelta =
      nextProps.messagesUi.length - this.props.messagesUi.length;
    const isOpeningGroup =
      messagesUiDelta > 0 &&
      nextProps.messagesUi.some(
        id =>
          !this.props.messagesUi.includes(id) &&
          nextProps.messagesUi.includes(id) &&
          this.props.visibleMessages.includes(id) &&
          nextProps.visibleMessages.includes(id)
      );

    this.shouldScrollBottom =
      (!this.props.initialized &&
        nextProps.initialized &&
        this.scrolledToBottom) ||
      isNewMessageEvaluationResult ||
      (this.scrolledToBottom && visibleMessagesDelta > 0 && !isOpeningGroup);
  }

  componentDidUpdate(prevProps) {
    this.maybeScrollToBottom();
    if (this?.outputNode?.lastChild) {
      this.lastMessageIntersectionObserver.observe(this.outputNode.lastChild);
    }

    if (prevProps.editorMode !== this.props.editorMode) {
      this.resizeObserver.observe(this.getElementToObserve());
    }
  }

  maybeScrollToBottom() {
    if (this.outputNode && this.shouldScrollBottom) {
      this.scrollToBottom();
    }
  }

  scrollToBottom() {
    if (this.outputNode.scrollHeight > this.outputNode.clientHeight) {
      this.outputNode.scrollTop = this.outputNode.scrollHeight;
    }

    this.scrolledToBottom = true;
  }

  getElementToObserve() {
    // In inline mode, we need to observe the output node parent, which contains both the
    // output and the input, so we don't trigger the resizeObserver callback when only the
    // output size changes (e.g. when a network request is expanded).
    return this.props.editorMode
      ? this.outputNode
      : this.outputNode?.parentNode;
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
      messagesRepeat,
      warningGroups,
      networkMessagesUpdate,
      networkMessageActiveTabId,
      serviceContainer,
      timestampsVisible,
      initialized,
    } = this.props;

    if (!initialized) {
      const numberMessagesFitViewport = getInitialMessageCountForViewport(
        window
      );
      if (numberMessagesFitViewport < visibleMessages.length) {
        visibleMessages = visibleMessages.slice(
          visibleMessages.length - numberMessagesFitViewport
        );
      }
    }

    const messageNodes = visibleMessages.map(messageId =>
      createElement(MessageContainer, {
        dispatch,
        key: messageId,
        messageId,
        serviceContainer,
        open: messagesUi.includes(messageId),
        payload: messagesPayload.get(messageId),
        timestampsVisible,
        repeat: messagesRepeat[messageId],
        badge: warningGroups.has(messageId)
          ? warningGroups.get(messageId).length
          : null,
        inWarningGroup:
          warningGroups && warningGroups.size > 0
            ? isMessageInWarningGroup(messages.get(messageId), visibleMessages)
            : false,
        networkMessageUpdate: networkMessagesUpdate[messageId],
        networkMessageActiveTabId,
        getMessage: () => messages.get(messageId),
        maybeScrollToBottom: this.maybeScrollToBottom,
      })
    );

    return dom.div(
      {
        className: "webconsole-output",
        role: "main",
        onContextMenu: this.onContextMenu,
        ref: node => {
          this.outputNode = node;
        },
      },
      messageNodes
    );
  }
}

function mapStateToProps(state, props) {
  return {
    initialized: state.ui.initialized,
    messages: getAllMessagesById(state),
    visibleMessages: getVisibleMessages(state),
    messagesUi: getAllMessagesUiById(state),
    messagesPayload: getAllMessagesPayloadById(state),
    messagesRepeat: getAllRepeatById(state),
    warningGroups: getAllWarningGroupsById(state),
    networkMessagesUpdate: getAllNetworkMessagesUpdateById(state),
    timestampsVisible: state.ui.timestampsVisible,
    networkMessageActiveTabId: state.ui.networkMessageActiveTabId,
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
