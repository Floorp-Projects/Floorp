/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createElement,
  createRef,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { initialize } = require("devtools/client/webconsole/actions/ui");
const LazyMessageList = require("devtools/client/webconsole/components/Output/LazyMessageList");

const {
  getMutableMessagesById,
  getAllMessagesUiById,
  getAllCssMessagesMatchingElements,
  getAllNetworkMessagesUpdateById,
  getLastMessageId,
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
loader.lazyRequireGetter(this, "flags", "devtools/shared/flags");

const { MESSAGE_TYPE } = require("devtools/client/webconsole/constants");

class ConsoleOutput extends Component {
  static get propTypes() {
    return {
      initialized: PropTypes.bool.isRequired,
      mutableMessages: PropTypes.object.isRequired,
      messageCount: PropTypes.number.isRequired,
      messagesUi: PropTypes.array.isRequired,
      serviceContainer: PropTypes.shape({
        attachRefToWebConsoleUI: PropTypes.func.isRequired,
        openContextMenu: PropTypes.func.isRequired,
        sourceMapURLService: PropTypes.object,
      }),
      dispatch: PropTypes.func.isRequired,
      timestampsVisible: PropTypes.bool,
      cssMatchingElements: PropTypes.object.isRequired,
      messagesRepeat: PropTypes.object.isRequired,
      warningGroups: PropTypes.object.isRequired,
      networkMessagesUpdate: PropTypes.object.isRequired,
      visibleMessages: PropTypes.array.isRequired,
      networkMessageActiveTabId: PropTypes.string.isRequired,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      editorMode: PropTypes.bool.isRequired,
      cacheGeneration: PropTypes.number.isRequired,
      disableVirtualization: PropTypes.bool,
      lastMessageId: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.maybeScrollToBottom = this.maybeScrollToBottom.bind(this);
    this.messageIdsToKeepAlive = new Set();
    this.ref = createRef();
    this.lazyMessageListRef = createRef();

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
    if (this.props.disableVirtualization) {
      return;
    }

    if (this.props.visibleMessages.length > 0) {
      this.scrollToBottom();
    }

    this.scrollDetectionIntersectionObserver = new IntersectionObserver(
      entries => {
        for (const entry of entries) {
          // Consider that we're not pinned to the bottom anymore if the bottom of the
          // scrollable area is within 10px of visible (half the typical element height.)
          this.scrolledToBottom = entry.intersectionRatio > 0;
        }
      },
      { root: this.outputNode, rootMargin: "10px" }
    );

    this.resizeObserver.observe(this.getElementToObserve());

    const { serviceContainer, onFirstMeaningfulPaint, dispatch } = this.props;
    serviceContainer.attachRefToWebConsoleUI(
      "outputScroller",
      this.ref.current
    );

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

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillUpdate(nextProps, nextState) {
    this.isUpdating = true;
    if (nextProps.cacheGeneration !== this.props.cacheGeneration) {
      this.messageIdsToKeepAlive = new Set();
    }

    if (nextProps.editorMode !== this.props.editorMode) {
      this.resizeObserver.disconnect();
    }

    const { outputNode } = this;
    if (!outputNode?.lastChild) {
      // Force a scroll to bottom when messages are added to an empty console.
      // This makes the console stay pinned to the bottom if a batch of messages
      // are added after a page refresh (Bug 1402237).
      this.shouldScrollBottom = true;
      this.scrolledToBottom = true;
      return;
    }

    const bottomBuffer = this.lazyMessageListRef.current.bottomBuffer;
    this.scrollDetectionIntersectionObserver.unobserve(bottomBuffer);

    // We need to scroll to the bottom if:
    // - we are reacting to "initialize" action, and we are already scrolled to the bottom
    // - the number of messages displayed changed and we are already scrolled to the
    //   bottom, but not if we are reacting to a group opening.
    // - the number of messages in the store changed and the new message is an evaluation
    //   result.

    const visibleMessagesDelta =
      nextProps.visibleMessages.length - this.props.visibleMessages.length;
    const messagesDelta = nextProps.messageCount - this.props.messageCount;
    // Evaluation results are never filtered out, so if it's in the store, it will be
    // visible in the output.
    const isNewMessageEvaluationResult =
      messagesDelta > 0 &&
      nextProps.lastMessageId &&
      nextProps.mutableMessages.get(nextProps.lastMessageId)?.type ===
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
    this.isUpdating = false;
    this.maybeScrollToBottom();
    if (this?.outputNode?.lastChild) {
      const bottomBuffer = this.lazyMessageListRef.current.bottomBuffer;
      this.scrollDetectionIntersectionObserver.observe(bottomBuffer);
    }

    if (prevProps.editorMode !== this.props.editorMode) {
      this.resizeObserver.observe(this.getElementToObserve());
    }
  }

  get outputNode() {
    return this.ref.current;
  }

  maybeScrollToBottom() {
    if (this.outputNode && this.shouldScrollBottom) {
      this.scrollToBottom();
    }
  }

  // The maybeScrollToBottom callback we provide to messages needs to be a little bit more
  // strict than the one we normally use, because they can potentially interrupt a user
  // scroll (between when the intersection observer registers the scroll break and when
  // a componentDidUpdate comes through to reconcile it.)
  maybeScrollToBottomMessageCallback(index) {
    if (
      this.outputNode &&
      this.shouldScrollBottom &&
      this.scrolledToBottom &&
      this.lazyMessageListRef.current?.isItemNearBottom(index)
    ) {
      this.scrollToBottom();
    }
  }

  scrollToBottom() {
    if (flags.testing && this.outputNode.hasAttribute("disable-autoscroll")) {
      return;
    }
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
    const {
      cacheGeneration,
      dispatch,
      visibleMessages,
      mutableMessages,
      messagesUi,
      cssMatchingElements,
      messagesRepeat,
      warningGroups,
      networkMessagesUpdate,
      networkMessageActiveTabId,
      serviceContainer,
      timestampsVisible,
    } = this.props;

    const renderMessage = (messageId, index) => {
      return createElement(MessageContainer, {
        dispatch,
        key: messageId,
        messageId,
        serviceContainer,
        open: messagesUi.includes(messageId),
        cssMatchingElements: cssMatchingElements.get(messageId),
        timestampsVisible,
        repeat: messagesRepeat[messageId],
        badge: warningGroups.has(messageId)
          ? warningGroups.get(messageId).length
          : null,
        inWarningGroup:
          warningGroups && warningGroups.size > 0
            ? isMessageInWarningGroup(
                mutableMessages.get(messageId),
                visibleMessages
              )
            : false,
        networkMessageUpdate: networkMessagesUpdate[messageId],
        networkMessageActiveTabId,
        getMessage: () => mutableMessages.get(messageId),
        maybeScrollToBottom: () =>
          this.maybeScrollToBottomMessageCallback(index),
        // Whenever a node is expanded, we want to make sure we keep the
        // message node alive so as to not lose the expanded state.
        setExpanded: () => this.messageIdsToKeepAlive.add(messageId),
      });
    };

    // scrollOverdrawCount tells the list to draw extra elements above and
    // below the scrollport so that we can avoid flashes of blank space
    // when scrolling. When `disableVirtualization` is passed we make it as large as the
    // number of messages to render them all and effectively disabling virtualization (this
    // should only be used for some actions that requires all the messages to be rendered
    // in the DOM, like "Copy All Messages").
    const scrollOverdrawCount = this.props.disableVirtualization
      ? visibleMessages.length
      : 20;

    const attrs = {
      className: "webconsole-output",
      role: "main",
      onContextMenu: this.onContextMenu,
      ref: this.ref,
    };
    if (flags.testing) {
      attrs["data-visible-messages"] = JSON.stringify(visibleMessages);
    }
    return dom.div(
      attrs,
      createElement(LazyMessageList, {
        viewportRef: this.ref,
        items: visibleMessages,
        itemDefaultHeight: 21,
        editorMode: this.props.editorMode,
        scrollOverdrawCount,
        ref: this.lazyMessageListRef,
        renderItem: renderMessage,
        itemsToKeepAlive: this.messageIdsToKeepAlive,
        serviceContainer,
        cacheGeneration,
        shouldScrollBottom: () => this.shouldScrollBottom && this.isUpdating,
      })
    );
  }
}

function mapStateToProps(state, props) {
  const mutableMessages = getMutableMessagesById(state);
  return {
    initialized: state.ui.initialized,
    cacheGeneration: state.ui.cacheGeneration,
    // We need to compute this so lifecycle methods can compare the global message count
    // on state change (since we can't do it with mutableMessagesById).
    messageCount: mutableMessages.size,
    mutableMessages,
    lastMessageId: getLastMessageId(state),
    visibleMessages: getVisibleMessages(state),
    messagesUi: getAllMessagesUiById(state),
    cssMatchingElements: getAllCssMessagesMatchingElements(state),
    messagesRepeat: getAllRepeatById(state),
    warningGroups: getAllWarningGroupsById(state),
    networkMessagesUpdate: getAllNetworkMessagesUpdateById(state),
    timestampsVisible: state.ui.timestampsVisible,
    networkMessageActiveTabId: state.ui.networkMessageActiveTabId,
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
