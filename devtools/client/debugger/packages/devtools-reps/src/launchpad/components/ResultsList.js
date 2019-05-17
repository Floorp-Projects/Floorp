/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const { Component, createFactory } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
const ImPropTypes = require("react-immutable-proptypes");

const Result = createFactory(require("./Result"));

class ResultsList extends Component {
  static get propTypes() {
    return {
      expressions: ImPropTypes.map.isRequired,
      showResultPacket: PropTypes.func.isRequired,
      hideResultPacket: PropTypes.func.isRequired,
    };
  }

  render() {
    const { expressions, showResultPacket, hideResultPacket } = this.props;

    return dom.div(
      { className: "expressions" },
      expressions
        .entrySeq()
        .toJS()
        .map(([key, expression]) =>
          Result({
            key,
            expression: expression.toJS(),
            showResultPacket: () => showResultPacket(key),
            hideResultPacket: () => hideResultPacket(key),
          })
        )
    );
  }
}

module.exports = ResultsList;
