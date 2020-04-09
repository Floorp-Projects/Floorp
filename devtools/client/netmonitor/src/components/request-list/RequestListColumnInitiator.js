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

    const lastFrameExists = cause && cause.lastFrame;
    if (lastFrameExists) {
      const { filename, lineNumber: _lineNumber } = cause.lastFrame;
      initiator = getUrlBaseName(filename);
      lineNumber = ":" + _lineNumber;
    }

    // Legacy server might send a numeric value. Display it as "unknown"
    const causeType = typeof cause.type === "string" ? cause.type : "unknown";
    const causeStr = lastFrameExists ? " (" + causeType + ")" : causeType;
    return dom.td(
      {
        className: "requests-list-column requests-list-initiator",
        title: initiator + lineNumber + causeStr,
      },
      dom.div(
        {
          className: "requests-list-initiator-lastframe",
          onMouseDown: onInitiatorBadgeMouseDown,
        },
        dom.span({ className: "requests-list-initiator-filename" }, initiator),
        dom.span({ className: "requests-list-initiator-line" }, lineNumber)
      ),
      dom.div({ className: "requests-list-initiator-cause" }, causeStr)
    );
  }
}

module.exports = RequestListColumnInitiator;
