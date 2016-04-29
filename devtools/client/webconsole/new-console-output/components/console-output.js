/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const DOM = React.DOM;

const MessageContainer = React.createFactory(require("devtools/client/webconsole/new-console-output/components/message-container").MessageContainer);

const ConsoleOutput = React.createClass({
  displayName: "ConsoleOutput",

  render() {
    let messageNodes = this.props.messages.map(function(message) {
      return (
        MessageContainer({ message })
      );
    });
    return (
      DOM.div({}, messageNodes)
    );
  }
});

const mapStateToProps = (state) => {
  return {
    messages: state.messages
  };
};

module.exports = connect(mapStateToProps)(ConsoleOutput);
