/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  getUrlBaseName,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class RequestListColumnInitiator extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      onInitiatorBadgeMouseDown: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.cause !== nextProps.item.cause;
  }

  render() {
    const {
      item: { cause },
      onInitiatorBadgeMouseDown,
    } = this.props;

    let initiator = "";
    let lineNumber = "";
    if (cause && cause.lastFrame) {
      const { filename, lineNumber: _lineNumber } = cause.lastFrame;
      initiator = getUrlBaseName(filename);
      lineNumber = ":" + _lineNumber;
    }

    return dom.td(
      {
        className: "requests-list-column requests-list-initiator",
        onMouseDown: onInitiatorBadgeMouseDown,
      },
      dom.div(
        { className: "requests-list-initiator-lastframe" },
        dom.span({ className: "requests-list-initiator-filename" }, initiator),
        dom.span({ className: "requests-list-initiator-line" }, lineNumber)
      )
    );
  }
}

module.exports = RequestListColumnInitiator;
