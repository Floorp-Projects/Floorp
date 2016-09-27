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
const MessageContainer = createFactory(require("devtools/client/webconsole/new-console-output/components/message-container").MessageContainer);

const ConsoleOutput = createClass({

  propTypes: {
    hudProxyClient: PropTypes.object.isRequired,
    messages: PropTypes.object.isRequired,
    messagesUi: PropTypes.object.isRequired,
    sourceMapService: PropTypes.object,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    openNetworkPanel: PropTypes.func.isRequired,
    openLink: PropTypes.func.isRequired,
  },

  displayName: "ConsoleOutput",

  componentWillUpdate() {
    let node = ReactDOM.findDOMNode(this);
    if (node.lastChild) {
      this.shouldScrollBottom = isScrolledToBottom(node.lastChild, node);
    }
  },

  componentDidUpdate() {
    if (this.shouldScrollBottom) {
      let node = ReactDOM.findDOMNode(this);
      node.scrollTop = node.scrollHeight;
    }
  },

  render() {
    let {
      dispatch,
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
        })
      );
    });
    return (
      dom.div({className: "webconsole-output"}, messageNodes)
    );
  }
});

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
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
