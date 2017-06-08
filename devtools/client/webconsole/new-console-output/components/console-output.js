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
  getAllMessagesUiById,
  getAllMessagesTableDataById,
  getAllNetworkMessagesUpdateById,
  getVisibleMessages,
  getAllRepeatById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const MessageContainer = createFactory(require("devtools/client/webconsole/new-console-output/components/message-container").MessageContainer);

const ConsoleOutput = createClass({

  displayName: "ConsoleOutput",

  propTypes: {
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
      return;
    }

    // Figure out if we are at the bottom. If so, then any new message should be scrolled
    // into view.
    const lastChild = outputNode.lastChild;
    const delta = nextProps.visibleMessages.length - this.props.visibleMessages.length;
    this.shouldScrollBottom = delta > 0 && isScrolledToBottom(lastChild, outputNode);
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
      messagesUi,
      messagesTableData,
      messagesRepeat,
      networkMessagesUpdate,
      serviceContainer,
      timestampsVisible,
    } = this.props;

    let messageNodes = visibleMessages.map((message) => {
      return (
        MessageContainer({
          dispatch,
          message,
          key: message.id,
          serviceContainer,
          open: messagesUi.includes(message.id),
          tableData: messagesTableData.get(message.id),
          indent: message.indent,
          timestampsVisible,
          repeat: messagesRepeat[message.id],
          networkMessageUpdate: networkMessagesUpdate[message.id],
        })
      );
    });

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
    visibleMessages: getVisibleMessages(state),
    messagesUi: getAllMessagesUiById(state),
    messagesTableData: getAllMessagesTableDataById(state),
    messagesRepeat: getAllRepeatById(state),
    networkMessagesUpdate: getAllNetworkMessagesUpdateById(state),
    timestampsVisible: state.ui.timestampsVisible,
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
