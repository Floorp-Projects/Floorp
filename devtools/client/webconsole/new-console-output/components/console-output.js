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

const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const MessageContainer = createFactory(require("devtools/client/webconsole/new-console-output/components/message-container").MessageContainer);

const ConsoleOutput = createClass({

  propTypes: {
    jsterm: PropTypes.object.isRequired,
    // This function is created in mergeProps
    openVariablesView: PropTypes.func.isRequired,
    messages: PropTypes.array.isRequired
  },

  displayName: "ConsoleOutput",

  componentWillUpdate() {
    // @TODO Move this to a parent component.
    let node = ReactDOM.findDOMNode(this).parentNode.parentNode.parentNode;
    if (node.lastChild) {
      this.shouldScrollBottom = isScrolledToBottom(node.lastChild, node);
    }
  },

  componentDidUpdate() {
    if (this.shouldScrollBottom) {
      let node = ReactDOM.findDOMNode(this).parentNode.parentNode.parentNode;
      node.scrollTop = node.scrollHeight;
    }
  },

  render() {
    let messageNodes = this.props.messages.map(function (message) {
      return (
        MessageContainer({ message })
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
    messages: getAllMessages(state)
  };
}

module.exports = connect(mapStateToProps)(ConsoleOutput);
