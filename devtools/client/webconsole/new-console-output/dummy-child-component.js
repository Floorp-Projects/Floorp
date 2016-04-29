/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const DOM = React.DOM;

var DummyChildComponent = React.createClass({
  displayName: "DummyChildComponent",

  render() {
    let messageNodes = this.props.messages.map(function(message) {
      return (
        DOM.div({}, message.arguments.join(" "))
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

module.exports = connect(mapStateToProps)(DummyChildComponent);
