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
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { getAllMessages, getAllMessagesUiById, getAllMessagesTableDataById } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { getScrollSetting } = require("devtools/client/webconsole/new-console-output/selectors/ui");
const MessageContainer = createFactory(require("devtools/client/webconsole/new-console-output/components/message-container").MessageContainer);

const ConsoleOutput = createClass({

  displayName: "ConsoleOutput",

  propTypes: {
    hudProxyClient: PropTypes.object.isRequired,
    messages: PropTypes.object.isRequired,
    messagesUi: PropTypes.object.isRequired,
    sourceMapService: PropTypes.object,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    openNetworkPanel: PropTypes.func.isRequired,
    openLink: PropTypes.func.isRequired,
    autoscroll: PropTypes.bool.isRequired,
  },

  componentDidMount() {
    scrollToBottom(this.outputNode);
  },

  componentWillUpdate(nextProps, nextState) {
    if (!this.outputNode) {
      return;
    }

    const outputNode = this.outputNode;

    // Figure out if we are at the bottom. If so, then any new message should be scrolled
    // into view.
    if (this.props.autoscroll && outputNode.lastChild) {
      this.shouldScrollBottom = isScrolledToBottom(outputNode.lastChild, outputNode);
    }
  },

  componentDidUpdate() {
    if (this.shouldScrollBottom) {
      scrollToBottom(this.outputNode);
    }
  },

  render() {
    let {
      dispatch,
      autoscroll,
      hudProxyClient,
      messages,
      messagesUi,
      messagesTableData,
      sourceMapService,
      onViewSourceInDebugger,
      openNetworkPanel,
      openLink,
    } = this.props;

    let messageNodes = messages.map((message) => {
      return (
        MessageContainer({
          dispatch,
          hudProxyClient,
          message,
          key: message.id,
          sourceMapService,
          onViewSourceInDebugger,
          openNetworkPanel,
          openLink,
          open: messagesUi.includes(message.id),
          tableData: messagesTableData.get(message.id),
          autoscroll,
        })
      );
    });
    return (
      dom.div({
        className: "webconsole-output",
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

function mapStateToProps(state) {
  return {
    messages: getAllMessages(state),
    messagesUi: getAllMessagesUiById(state),
    messagesTableData: getAllMessagesTableDataById(state),
    autoscroll: getScrollSetting(state),
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
